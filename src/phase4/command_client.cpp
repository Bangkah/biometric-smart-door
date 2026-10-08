#include "command_client.h"
#include "command_policy.h"
#include "access_policy.h"
#include "config.h"  // include/config.h, dijangkau lewat -I global (bukan path relatif src/)
#include "../event_logger.h"
#include "../network_manager.h"
#include "../time_sync.h"
#include "../backoff.h"
#include "../url_utils.h"

#include <Preferences.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <mbedtls/md.h>
#include <string.h>

CommandClient commandClient;

// ------------------------------------------------------------
// begin()
// ------------------------------------------------------------
void CommandClient::begin() {
  _enabled = false;

  if (_bridgeMutex == nullptr) _bridgeMutex = xSemaphoreCreateMutex();
  if (_bridgeMutex == nullptr) {
    Serial.println("[COMMAND_CLIENT] GAGAL membuat mutex bridge!");
    return;
  }

  if (!NETWORK_ENABLED || strlen(WIFI_SSID) == 0) {
    Serial.println("[COMMAND_CLIENT] Jaringan tidak dikonfigurasi -> nonaktif.");
    return;
  }
  if (strlen(DEVICE_HMAC_SECRET) == 0) {
    Serial.println("[COMMAND_CLIENT] DEVICE_HMAC_SECRET kosong -> command client NONAKTIF "
                   "(fail-closed; isi di secrets.h dari hasil registrasi device Phase 4).");
    return;
  }
  if (strlen(DEVICE_API_TOKEN) == 0) {
    Serial.println("[COMMAND_CLIENT] DEVICE_API_TOKEN kosong -> nonaktif.");
    return;
  }

  _https = (strncmp(BACKEND_EVENTS_ENDPOINT, "https://", 8) == 0);
  bool plain = (strncmp(BACKEND_EVENTS_ENDPOINT, "http://", 7) == 0);
  if (plain && !BACKEND_ALLOW_PLAIN_HTTP) {
    Serial.println("[COMMAND_CLIENT] DITOLAK: http:// tanpa TLS (kebijakan sama seperti ApiClient).");
    return;
  }
  if (_https && strlen(BACKEND_ROOT_CA) == 0 && !BACKEND_TLS_INSECURE) {
    Serial.println("[COMMAND_CLIENT] DITOLAK: https:// tanpa BACKEND_ROOT_CA.");
    return;
  }

  char baseUrl[96];
  if (!urlutils::deriveBaseUrl(BACKEND_EVENTS_ENDPOINT, baseUrl, sizeof(baseUrl))) {
    Serial.println("[COMMAND_CLIENT] GAGAL menurunkan base URL dari BACKEND_EVENTS_ENDPOINT -> nonaktif.");
    return;
  }
  _pollUrl = String(baseUrl) + "/api/v1/commands/poll";
  _ackUrlPrefix = String(baseUrl) + "/api/v1/commands/";

  _nextPollAt = millis();
  _enabled = true;
  Serial.printf("[COMMAND_CLIENT] Siap. poll=%s, interval=%lu s\n", _pollUrl.c_str(),
                COMMAND_POLL_INTERVAL_MS / 1000UL);
}

// ------------------------------------------------------------
// hmacSha256Hex(): HMAC-SHA256 memakai DEVICE_HMAC_SECRET, hasil hex huruf kecil.
// ------------------------------------------------------------
bool CommandClient::hmacSha256Hex(const char* message, char* outHex, size_t outHexLen) {
  if (outHexLen < COMMAND_SIGNATURE_HEX_LEN + 1) return false;

  const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (info == nullptr) return false;

  unsigned char digest[32];
  int rc = mbedtls_md_hmac(info, reinterpret_cast<const unsigned char*>(DEVICE_HMAC_SECRET),
                            strlen(DEVICE_HMAC_SECRET), reinterpret_cast<const unsigned char*>(message),
                            strlen(message), digest);
  if (rc != 0) return false;

  for (int i = 0; i < 32; i++) {
    snprintf(outHex + (i * 2), 3, "%02x", digest[i]);
  }
  outHex[64] = '\0';
  return true;
}

// ------------------------------------------------------------
// scheduleRetry()
// ------------------------------------------------------------
void CommandClient::scheduleRetry(unsigned long now) {
  unsigned long delay = computeBackoffDelay(_failCount > 0 ? _failCount - 1 : 0, COMMAND_RETRY_BASE_MS,
                                             COMMAND_RETRY_MAX_MS);
  _nextPollAt = now + delay;
}

// ------------------------------------------------------------
// Penanganan satu command hasil parsing JSON. Fungsi bebas (bukan method)
// supaya JsonObjectConst (tipe ArduinoJson) tidak perlu bocor ke header.
// Mengembalikan true bila command SUDAH di-ack di dalam fungsi ini (ALLOW/
// REVOKE_SLOT, atau ditolak karena signature/expiry tidak valid) -- caller
// (pollOnce) tidak perlu berbuat apa-apa lagi untuk command tsb. Untuk
// REMOTE_UNLOCK yang diterima, ack DITUNDA sampai state machine benar-benar
// mengeksekusinya (lihat drainUnlockResultIfReady), jadi return false.
// ------------------------------------------------------------
namespace {

bool processCommand(JsonObjectConst cmd, CommandClient& self,
                     SemaphoreHandle_t bridgeMutex, volatile bool& unlockPending,
                     char* unlockCommandId, size_t unlockCommandIdLen, uint32_t& unlockExpiresAt) {
  const char* id = cmd["id"] | "";
  const char* type = cmd["command_type"] | "";
  const char* signature = cmd["signature"] | "";
  uint32_t issuedAt = cmd["issued_at"] | 0;
  uint32_t expiresAt = cmd["expires_at"] | 0;

  if (id[0] == '\0' || type[0] == '\0' || signature[0] == '\0') {
    Serial.println("[COMMAND_CLIENT] Command dari server tidak lengkap, dilewati.");
    return true;  // tidak ada id valid untuk di-ack; tidak ada yang bisa dilakukan
  }

  JsonObjectConst payloadObj = cmd["payload"];
  bool hasSlot = payloadObj["slot"].is<int32_t>();
  int32_t slot = hasSlot ? payloadObj["slot"].as<int32_t>() : 0;

  char canonicalPayload[32];
  if (!commandpolicy::canonicalPayload(type, hasSlot, slot, canonicalPayload, sizeof(canonicalPayload))) {
    Serial.printf("[COMMAND_CLIENT] command_type/payload tidak valid: %s -> ditolak.\n", type);
    eventLogger.logEvent(EventType::COMMAND_REJECTED, -1, "Unknown command_type/payload");
    self.ackCommand(id, "failed", "unknown command_type or invalid payload");
    return true;
  }

  char sigString[160];
  if (!commandpolicy::buildSignatureString(id, type, issuedAt, expiresAt, canonicalPayload, sigString,
                                            sizeof(sigString))) {
    self.ackCommand(id, "failed", "internal: signature string too long");
    return true;
  }

  char computedHex[COMMAND_SIGNATURE_HEX_LEN + 1];
  if (!self.hmacSha256Hex(sigString, computedHex, sizeof(computedHex)) ||
      !commandpolicy::constantTimeHexEqual(computedHex, signature)) {
    Serial.printf("[COMMAND_CLIENT] *** SIGNATURE TIDAK VALID *** untuk command #%s (%s) -> DITOLAK, "
                  "TIDAK dieksekusi.\n", id, type);
    eventLogger.logEvent(EventType::COMMAND_REJECTED, -1, "Invalid HMAC signature");
    self.ackCommand(id, "failed", "invalid signature");
    return true;
  }

  uint32_t nowUnix = static_cast<uint32_t>(timeSync.nowUnix());
  if (!commandpolicy::isCommandValidNow(nowUnix, expiresAt)) {
    Serial.printf("[COMMAND_CLIENT] Command #%s (%s) kadaluwarsa/waktu tidak diketahui -> ditolak.\n", id, type);
    eventLogger.logEvent(EventType::COMMAND_REJECTED, -1, "Expired or clock unsynced");
    self.ackCommand(id, "expired", "expired before execution, or device clock unsynced");
    return true;
  }

  // ---- Signature & masa berlaku valid -> eksekusi sesuai tipe ----
  if (strcmp(type, "ALLOW_SLOT") == 0 || strcmp(type, "REVOKE_SLOT") == 0) {
    bool allow = (strcmp(type, "ALLOW_SLOT") == 0);
    accessPolicy.setSlotAllowed(slot, allow);
    eventLogger.logEvent(EventType::USER_SYNC_APPLIED, static_cast<int16_t>(slot),
                          allow ? "Slot allowed (remote)" : "Slot revoked (remote)");
    self.ackCommand(id, "executed", allow ? "slot allowed" : "slot revoked");
    return true;
  }

  if (strcmp(type, "REMOTE_UNLOCK") == 0) {
    bool accepted = false;
    if (xSemaphoreTake(bridgeMutex, pdMS_TO_TICKS(EVENT_LOG_LOCK_TIMEOUT_MS)) == pdTRUE) {
      if (!unlockPending) {
        strncpy(unlockCommandId, id, unlockCommandIdLen - 1);
        unlockCommandId[unlockCommandIdLen - 1] = '\0';
        unlockExpiresAt = expiresAt;
        unlockPending = true;
        accepted = true;
      }
      xSemaphoreGive(bridgeMutex);
    }

    if (accepted) {
      Serial.printf("[COMMAND_CLIENT] REMOTE_UNLOCK #%s diterima & valid -> dititipkan ke state machine.\n", id);
      // Ack DITUNDA -- akan dikirim oleh drainUnlockResultIfReady() setelah
      // state machine benar-benar mengeksekusi (atau bridge expire sendiri).
      return false;
    } else {
      Serial.printf("[COMMAND_CLIENT] REMOTE_UNLOCK #%s ditolak: masih ada permintaan lain pending.\n", id);
      self.ackCommand(id, "failed", "another remote unlock already pending on this device");
      return true;
    }
  }

  // Seharusnya tidak tercapai (canonicalPayload sudah memvalidasi command_type).
  self.ackCommand(id, "failed", "unhandled command_type");
  return true;
}

}  // namespace

// ------------------------------------------------------------
// pollOnce()
// ------------------------------------------------------------
void CommandClient::pollOnce() {
  WiFiClientSecure secureClient;
  WiFiClient plainClient;
  HTTPClient http;
  http.setConnectTimeout(COMMAND_HTTP_CONNECT_TIMEOUT_MS);
  http.setTimeout(COMMAND_HTTP_REQUEST_TIMEOUT_MS);
  http.setReuse(false);

  bool ok;
  if (_https) {
    if (strlen(BACKEND_ROOT_CA) > 0) secureClient.setCACert(BACKEND_ROOT_CA);
    else secureClient.setInsecure();
    ok = http.begin(secureClient, _pollUrl);
  } else {
    ok = http.begin(plainClient, _pollUrl);
  }
  if (!ok) {
    Serial.println("[COMMAND_CLIENT] http.begin() gagal untuk poll URL.");
    if (_failCount < 250) _failCount++;
    scheduleRetry(millis());
    return;
  }

  http.addHeader("Authorization", String("Bearer ") + DEVICE_API_TOKEN);
  http.addHeader("X-Device-Id", DEVICE_ID);
  int code = http.GET();
  _lastHttpCode = code;

  if (code != 200) {
    Serial.printf("[COMMAND_CLIENT] Poll GAGAL (HTTP %d).\n", code);
    http.end();
    if (_failCount < 250) _failCount++;
    scheduleRetry(millis());
    return;
  }

  String body = http.getString();
  http.end();

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    Serial.printf("[COMMAND_CLIENT] Respons JSON tidak valid: %s\n", err.c_str());
    if (_failCount < 250) _failCount++;
    scheduleRetry(millis());
    return;
  }

  JsonArrayConst commands = doc["commands"].as<JsonArrayConst>();
  for (JsonObjectConst cmd : commands) {
    processCommand(cmd, *this, _bridgeMutex, _unlockPending, _unlockCommandId, sizeof(_unlockCommandId),
                    _unlockExpiresAt);
  }

  _failCount = 0;
  _everSucceeded = true;
  _lastSuccessAt = millis();
  _nextPollAt = millis() + COMMAND_POLL_INTERVAL_MS;
}

// ------------------------------------------------------------
// ackCommand()
// ------------------------------------------------------------
void CommandClient::ackCommand(const char* commandId, const char* status, const char* detail) {
  WiFiClientSecure secureClient;
  WiFiClient plainClient;
  HTTPClient http;
  http.setConnectTimeout(COMMAND_HTTP_CONNECT_TIMEOUT_MS);
  http.setTimeout(COMMAND_HTTP_REQUEST_TIMEOUT_MS);
  http.setReuse(false);

  String url = _ackUrlPrefix + commandId + "/ack";
  bool ok;
  if (_https) {
    if (strlen(BACKEND_ROOT_CA) > 0) secureClient.setCACert(BACKEND_ROOT_CA);
    else secureClient.setInsecure();
    ok = http.begin(secureClient, url);
  } else {
    ok = http.begin(plainClient, url);
  }
  if (!ok) {
    Serial.println("[COMMAND_CLIENT] http.begin() gagal untuk ack URL.");
    return;
  }

  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", String("Bearer ") + DEVICE_API_TOKEN);
  http.addHeader("X-Device-Id", DEVICE_ID);

  JsonDocument doc;
  doc["status"] = status;
  doc["detail"] = detail;
  String payload;
  serializeJson(doc, payload);

  int code = http.POST(payload);
  if (code >= 200 && code < 300) {
    if (strcmp(status, "executed") == 0) _totalExecuted++; else _totalRejected++;
  } else {
    Serial.printf("[COMMAND_CLIENT] Ack #%s GAGAL (HTTP %d) -- backend mungkin menganggap command masih "
                  "pending; tidak fatal, akan tampak di dashboard sebagai stale/expired.\n", commandId, code);
  }
  http.end();
}

// ------------------------------------------------------------
// checkUnlockBridgeExpiry() / drainUnlockResultIfReady()
// ------------------------------------------------------------
void CommandClient::checkUnlockBridgeExpiry() {
  if (xSemaphoreTake(_bridgeMutex, pdMS_TO_TICKS(EVENT_LOG_LOCK_TIMEOUT_MS)) != pdTRUE) return;

  if (_unlockPending) {
    uint32_t nowUnix = static_cast<uint32_t>(timeSync.nowUnix());
    if (nowUnix != 0 && nowUnix >= _unlockExpiresAt) {
      Serial.printf("[COMMAND_CLIENT] REMOTE_UNLOCK #%s kadaluwarsa sebelum sempat dieksekusi "
                    "(device mungkin sedang ERROR_SAFE) -> dilaporkan expired.\n", _unlockCommandId);
      strncpy(_unlockResultCommandId, _unlockCommandId, sizeof(_unlockResultCommandId) - 1);
      _unlockResultSuccess = false;
      strncpy(_unlockResultDetail, "expired waiting for IDLE state", sizeof(_unlockResultDetail) - 1);
      _unlockResultReady = true;
      _unlockPending = false;
    }
  }
  xSemaphoreGive(_bridgeMutex);
}

void CommandClient::drainUnlockResultIfReady() {
  bool ready = false;
  char id[COMMAND_ID_MAX_LEN];
  bool success = false;
  char detail[64];

  if (xSemaphoreTake(_bridgeMutex, pdMS_TO_TICKS(EVENT_LOG_LOCK_TIMEOUT_MS)) == pdTRUE) {
    if (_unlockResultReady) {
      ready = true;
      strncpy(id, _unlockResultCommandId, sizeof(id) - 1);
      id[sizeof(id) - 1] = '\0';
      success = _unlockResultSuccess;
      strncpy(detail, _unlockResultDetail, sizeof(detail) - 1);
      detail[sizeof(detail) - 1] = '\0';
      _unlockResultReady = false;
    }
    xSemaphoreGive(_bridgeMutex);
  }

  if (ready) {
    ackCommand(id, success ? "executed" : "failed", detail);
  }
}

// ------------------------------------------------------------
// update(): dipanggil ~tiap NET_TASK_TICK_MS oleh network task.
// ------------------------------------------------------------
void CommandClient::update() {
  if (!_enabled) return;

  // Dua hal ini TIDAK bergantung timing poll -- harus tetap jalan tiap tick
  // supaya hasil eksekusi (atau expiry lokal) selalu segera dilaporkan.
  drainUnlockResultIfReady();
  checkUnlockBridgeExpiry();

  if (!networkManager.isConnected() || !timeSync.isSynced()) return;  // fail-closed: butuh waktu valid

  unsigned long now = millis();
  if (static_cast<long>(now - _nextPollAt) < 0) return;

  pollOnce();
}

// ------------------------------------------------------------
// Jembatan publik (dipanggil state_machine dari LOOP TASK)
// ------------------------------------------------------------
bool CommandClient::consumeUnlockRequest(char* idOut, size_t idOutLen) {
  if (!_enabled || _bridgeMutex == nullptr) return false;

  bool found = false;
  if (xSemaphoreTake(_bridgeMutex, pdMS_TO_TICKS(EVENT_LOG_LOCK_TIMEOUT_MS)) == pdTRUE) {
    if (_unlockPending) {
      strncpy(idOut, _unlockCommandId, idOutLen - 1);
      idOut[idOutLen - 1] = '\0';
      _unlockPending = false;  // dikonsumsi -> slot bebas untuk command berikutnya
      found = true;
    }
    xSemaphoreGive(_bridgeMutex);
  }
  return found;
}

void CommandClient::reportUnlockResult(const char* commandId, bool success, const char* detail) {
  if (_bridgeMutex == nullptr) return;

  if (xSemaphoreTake(_bridgeMutex, pdMS_TO_TICKS(EVENT_LOG_LOCK_TIMEOUT_MS)) == pdTRUE) {
    strncpy(_unlockResultCommandId, commandId, sizeof(_unlockResultCommandId) - 1);
    _unlockResultCommandId[sizeof(_unlockResultCommandId) - 1] = '\0';
    _unlockResultSuccess = success;
    strncpy(_unlockResultDetail, detail, sizeof(_unlockResultDetail) - 1);
    _unlockResultDetail[sizeof(_unlockResultDetail) - 1] = '\0';
    _unlockResultReady = true;
    xSemaphoreGive(_bridgeMutex);
  }
}

// ------------------------------------------------------------
// printStatus(): bagian dari perintah debugger 'w'.
// ------------------------------------------------------------
void CommandClient::printStatus() const {
  Serial.println("---------- REMOTE COMMANDS (Phase 4) ----------");
  if (!_enabled) {
    Serial.println("Status    : NONAKTIF (lihat pesan [COMMAND_CLIENT] saat boot)");
    return;
  }
  Serial.printf("Poll URL  : %s\n", _pollUrl.c_str());
  Serial.printf("Interval  : %lu s\n", COMMAND_POLL_INTERVAL_MS / 1000UL);
  Serial.printf("Terakhir  : HTTP %d\n", _lastHttpCode);
  if (_everSucceeded) {
    Serial.printf("Sukses terakhir: %lu s lalu\n", (millis() - _lastSuccessAt) / 1000UL);
  }
  if (_failCount > 0) {
    Serial.printf("Gagal beruntun: %u\n", (unsigned)_failCount);
  }
  Serial.printf("Total dieksekusi: %lu, ditolak: %lu\n", (unsigned long)_totalExecuted,
                (unsigned long)_totalRejected);
  Serial.printf("Bridge unlock pending: %s\n", _unlockPending ? "YA" : "tidak");
}
