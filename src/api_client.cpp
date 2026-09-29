#include "api_client.h"
#include "config.h"
#include "event_logger.h"
#include "network_manager.h"
#include "time_sync.h"
#include "backoff.h"

#include <Preferences.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <string.h>
#include <stdlib.h>

ApiClient apiClient;

const char* syncResultToString(SyncResult result) {
  switch (result) {
    case SyncResult::NONE:            return "NONE";
    case SyncResult::SUCCESS:         return "SUCCESS";
    case SyncResult::NOTHING_TO_SYNC: return "NOTHING_TO_SYNC";
    case SyncResult::NO_NETWORK:      return "NO_NETWORK";
    case SyncResult::HTTP_ERROR:      return "HTTP_ERROR";
    default:                          return "UNKNOWN";
  }
}

// ------------------------------------------------------------
// begin(): validasi konfigurasi. Sengaja "fail closed": bila kebijakan
// keamanan tidak terpenuhi, upload dimatikan (perangkat tetap berfungsi
// penuh secara lokal — SRS 13.6 Backend Failure).
// ------------------------------------------------------------
void ApiClient::begin() {
  _enabled = false;
  _heartbeatAt = millis();

  if (!NETWORK_ENABLED || strlen(WIFI_SSID) == 0) {
    Serial.println("[API_CLIENT] Jaringan tidak dikonfigurasi -> upload dinonaktifkan (offline-only).");
    return;
  }
  if (strlen(DEVICE_API_TOKEN) == 0) {
    Serial.println("[API_CLIENT] DEVICE_API_TOKEN kosong -> upload dinonaktifkan.");
    return;
  }

  _https = (strncmp(BACKEND_EVENTS_ENDPOINT, "https://", 8) == 0);
  bool plain = (strncmp(BACKEND_EVENTS_ENDPOINT, "http://", 7) == 0);

  if (!_https && !plain) {
    Serial.println("[API_CLIENT] BACKEND_EVENTS_ENDPOINT harus diawali https:// -> upload dinonaktifkan.");
    return;
  }
  if (plain && !BACKEND_ALLOW_PLAIN_HTTP) {
    Serial.println("[API_CLIENT] DITOLAK: http:// tanpa TLS melanggar SRS NFR-001. "
                   "Gunakan https:// atau build env esp32dev-bench untuk pengujian lokal.");
    return;
  }
  if (_https && strlen(BACKEND_ROOT_CA) == 0 && !BACKEND_TLS_INSECURE) {
    Serial.println("[API_CLIENT] DITOLAK: https:// tanpa BACKEND_ROOT_CA. Isi root CA di secrets.h "
                   "(atau -D BACKEND_TLS_INSECURE=1 khusus dev).");
    return;
  }
  if (plain) {
    Serial.println("[API_CLIENT] PERINGATAN: HTTP tanpa TLS aktif (hanya untuk bench/dev!).");
  } else if (strlen(BACKEND_ROOT_CA) == 0) {
    Serial.println("[API_CLIENT] PERINGATAN: TLS TANPA verifikasi sertifikat (BACKEND_TLS_INSECURE) — dev saja!");
  }

  loadState();
  _enabled = true;
  Serial.printf("[API_CLIENT] Siap. epoch=%lu, lastSyncedSequence=%lu, interval=%lu s, batch=%d.\n",
                (unsigned long)_epoch, (unsigned long)_lastSyncedSequence,
                SYNC_INTERVAL_MS / 1000UL, SYNC_BATCH_MAX_ENTRIES);
}

void ApiClient::loadState() {
  Preferences prefs;
  prefs.begin(API_CLIENT_NVS_NAMESPACE, false);
  _epoch = prefs.getUInt("epoch", 0);
  _lastSyncedSequence = prefs.getUInt("lastSeq", 0);
  prefs.end();
}

void ApiClient::saveState() {
  Preferences prefs;
  prefs.begin(API_CLIENT_NVS_NAMESPACE, false);
  prefs.putUInt("epoch", _epoch);
  prefs.putUInt("lastSeq", _lastSyncedSequence);
  prefs.end();
}

// ------------------------------------------------------------
// update(): dipanggil ~tiap NET_TASK_TICK_MS oleh network task.
// ------------------------------------------------------------
void ApiClient::update() {
  _heartbeatAt = millis();

  if (!_enabled) {
    if (_manualRequest) {
      _manualRequest = false;
      Serial.println("[API_CLIENT] Sync manual diabaikan: upload nonaktif (lihat pesan saat boot).");
    }
    return;
  }

  bool manual = _manualRequest;
  unsigned long now = millis();

  // Belum waktunya (dan tidak diminta manual)? Keluar tanpa menyentuh apa pun.
  if (!manual && static_cast<long>(now - _nextAttemptAt) < 0) return;

  if (!networkManager.isConnected()) {
    if (manual) {
      _manualRequest = false;
      _lastResult = SyncResult::NO_NETWORK;
      Serial.println("[API_CLIENT] Sync manual ditunda: Wi-Fi belum terhubung.");
    }
    return;
  }
  if (SYNC_REQUIRE_TIME && !timeSync.isSynced()) {
    if (manual) {
      _manualRequest = false;
      _lastResult = SyncResult::NO_NETWORK;
      Serial.println("[API_CLIENT] Sync manual ditunda: NTP belum sinkron.");
    }
    return;
  }

  _manualRequest = false;
  syncOnce(manual);
}

void ApiClient::scheduleRetry(unsigned long now) {
  unsigned long delayMs = computeBackoffDelay(_failCount > 0 ? _failCount - 1 : 0, SYNC_RETRY_BASE_MS, SYNC_RETRY_MAX_MS);
  _nextAttemptAt = now + delayMs;
  Serial.printf("[API_CLIENT] Retry otomatis dalam %lu s (kegagalan beruntun: %u).\n",
                delayMs / 1000UL, (unsigned)_failCount);
}

// ------------------------------------------------------------
// syncOnce(): kirim SATU batch. Bila masih ada backlog, jadwal berikutnya
// = segera (tick berikutnya), sehingga backlog terkuras batch demi batch
// sambil tetap memberi napas ke task lain.
// ------------------------------------------------------------
void ApiClient::syncOnce(bool manual) {
  unsigned long now = millis();

  uint32_t newest = 0, count = 0, epoch = 0;
  if (!eventLogger.snapshot(newest, count, epoch)) {
    _nextAttemptAt = now + 1000;
    return;
  }

  // Ring buffer dihapus/dibuat ulang -> sequence mulai dari 1 lagi.
  // Penanda lama tidak berlaku; tanpa reset ini event baru akan terlewat.
  if (epoch != _epoch) {
    Serial.printf("[API_CLIENT] Epoch log berubah (%lu -> %lu): penanda sinkron direset.\n",
                  (unsigned long)_epoch, (unsigned long)epoch);
    _epoch = epoch;
    _lastSyncedSequence = 0;
    saveState();
  }
  if (_lastSyncedSequence > newest) {  // seharusnya tak terjadi; jaga-jaga data NVS janggal
    Serial.println("[API_CLIENT] PERINGATAN: penanda > sequence terbaru, direset ke 0.");
    _lastSyncedSequence = 0;
    saveState();
  }

  if (newest <= _lastSyncedSequence) {
    _lastResult = SyncResult::NOTHING_TO_SYNC;
    _nextAttemptAt = now + SYNC_INTERVAL_MS;
    if (manual) Serial.println("[API_CLIENT] Tidak ada event baru untuk disinkronkan.");
    return;
  }

  // Event yang sudah tertimpa ring buffer sebelum sempat terkirim (offline lama).
  uint32_t oldest = (count > 0) ? (newest - count + 1) : newest;
  uint32_t after = _lastSyncedSequence;
  uint32_t missed = 0;
  if (after + 1 < oldest) {
    missed = oldest - (after + 1);
    after = oldest - 1;
    Serial.printf("[API_CLIENT] PERINGATAN: %lu event tertimpa ring buffer sebelum sempat terkirim.\n",
                  (unsigned long)missed);
  }

  EventRecord batch[SYNC_BATCH_MAX_ENTRIES];
  uint32_t n = eventLogger.collectAfter(after, batch, SYNC_BATCH_MAX_ENTRIES);
  if (n == 0) {
    _nextAttemptAt = now + 2000;
    return;
  }

  // ---- Bangun payload JSON (SRS 10.2) ----
  JsonDocument doc;
  doc["device_id"] = DEVICE_ID;
  doc["firmware_version"] = FIRMWARE_VERSION;
  doc["log_epoch"] = epoch;
  uint32_t sentAt = static_cast<uint32_t>(timeSync.nowUnix());
  if (sentAt != 0) doc["sent_at"] = sentAt; else doc["sent_at"] = nullptr;
  doc["missed_events"] = missed;

  JsonArray events = doc["events"].to<JsonArray>();
  uint16_t currentBoot = eventLogger.bootId();

  for (uint32_t i = 0; i < n; i++) {
    const EventRecord &r = batch[i];
    // Salin field packed ke lokal (tidak boleh di-bind ke reference).
    uint32_t seq = r.sequence;
    uint16_t boot = r.bootId;
    uint32_t upMs = r.timestampMs;
    uint32_t ts = r.unixTimestamp;
    int16_t detail = r.detail;

    const char* source = "ntp";
    if (ts == 0) {
      // NTP belum sinkron saat event dicatat. Untuk event SESI BOOT INI waktu
      // riilnya dapat diperkirakan dari uptime; sesi lama tidak bisa dipulihkan.
      if (boot == currentBoot) {
        ts = timeSync.estimateUnixForUptime(upMs);
        source = (ts != 0) ? "estimated" : "unsynced";
      } else {
        source = "unsynced";
      }
    }

    JsonObject o = events.add<JsonObject>();
    o["sequence"] = seq;
    o["boot_id"] = boot;
    o["uptime_ms"] = upMs;
    if (ts != 0) o["timestamp"] = ts; else o["timestamp"] = nullptr;
    o["time_source"] = source;
    o["type"] = eventTypeToString(static_cast<EventType>(r.type));
    o["detail"] = detail;
    o["message"] = r.message;
  }

  if (doc.overflowed()) {
    Serial.println("[API_CLIENT] GAGAL: JSON overflow (memori tidak cukup).");
    _lastResult = SyncResult::HTTP_ERROR;
    if (_failCount < 250) _failCount++;
    scheduleRetry(millis());
    return;
  }

  size_t len = measureJson(doc);
  char* payload = static_cast<char*>(malloc(len + 1));
  if (payload == nullptr) {
    Serial.println("[API_CLIENT] GAGAL: malloc payload.");
    _lastResult = SyncResult::HTTP_ERROR;
    if (_failCount < 250) _failCount++;
    scheduleRetry(millis());
    return;
  }
  serializeJson(doc, payload, len + 1);

  uint32_t firstSeq = batch[0].sequence;
  uint32_t lastSeq = batch[n - 1].sequence;
  Serial.printf("[API_CLIENT] Mengirim %lu event (sequence #%lu s/d #%lu, %u byte) ...\n",
                (unsigned long)n, (unsigned long)firstSeq, (unsigned long)lastSeq, (unsigned)len);

  String errInfo;
  int code = httpPost(reinterpret_cast<const uint8_t*>(payload), len, errInfo);
  free(payload);

  now = millis();
  _lastHttpCode = code;
  bool success = (code >= 200 && code < 300) || code == 409;  // 409 = sudah pernah diterima

  if (success) {
    _lastSyncedSequence = lastSeq;
    saveState();
    _failCount = 0;
    _lastResult = SyncResult::SUCCESS;
    _lastSuccessAt = now;
    _everSucceeded = true;
    _totalSent += n;
    Serial.printf("[API_CLIENT] Sukses (HTTP %d). lastSyncedSequence = %lu.\n",
                  code, (unsigned long)_lastSyncedSequence);
    // Masih ada backlog? lanjut segera; jika tidak, tunggu interval berkala.
    _nextAttemptAt = (lastSeq < newest) ? now : (now + SYNC_INTERVAL_MS);
  } else {
    _lastResult = SyncResult::HTTP_ERROR;
    if (_failCount < 250) _failCount++;
    Serial.printf("[API_CLIENT] GAGAL (HTTP %d) %s\n", code, errInfo.c_str());
    if (code == 401 || code == 403) {
      Serial.println("[API_CLIENT] Backend menolak token perangkat. Event TIDAK dibuang; "
                     "periksa DEVICE_API_TOKEN / registrasi perangkat.");
    }
    // Penanda sengaja TIDAK maju -> batch yang sama dicoba lagi (tanpa kehilangan).
    scheduleRetry(now);
  }
}

// ------------------------------------------------------------
// httpPost(): satu request. Semua tahap dibatasi timeout sehingga network
// task tidak bisa menggantung tanpa batas.
// ------------------------------------------------------------
int ApiClient::httpPost(const uint8_t* body, size_t len, String &errInfo) {
  WiFiClientSecure secureClient;
  WiFiClient plainClient;
  HTTPClient http;

  http.setConnectTimeout(HTTP_CONNECT_TIMEOUT_MS);
  http.setTimeout(HTTP_REQUEST_TIMEOUT_MS);
  http.setReuse(false);

  bool ok;
  if (_https) {
    if (strlen(BACKEND_ROOT_CA) > 0) {
      secureClient.setCACert(BACKEND_ROOT_CA);
    } else {
      secureClient.setInsecure();  // hanya tercapai bila BACKEND_TLS_INSECURE=1 (divalidasi di begin())
    }
    ok = http.begin(secureClient, BACKEND_EVENTS_ENDPOINT);
  } else {
    ok = http.begin(plainClient, BACKEND_EVENTS_ENDPOINT);
  }
  if (!ok) {
    errInfo = "http.begin() gagal — periksa BACKEND_EVENTS_ENDPOINT";
    return -1000;
  }

  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", String("Bearer ") + DEVICE_API_TOKEN);
  http.addHeader("X-Device-Id", DEVICE_ID);

  int code = http.POST(const_cast<uint8_t*>(body), len);

  if (code < 0) {
    errInfo = HTTPClient::errorToString(code);  // timeout / DNS / TLS
  } else if (code < 200 || code >= 300) {
    errInfo = http.getString();
    if (errInfo.length() > 120) errInfo = errInfo.substring(0, 120);
  }
  http.end();
  return code;
}

// ------------------------------------------------------------
// printStatus(): perintah debugger 'w' (bagian sync). Token tidak dicetak.
// ------------------------------------------------------------
void ApiClient::printStatus() const {
  Serial.println("---------- BACKEND SYNC ----------");
  if (!_enabled) {
    Serial.println("Status    : NONAKTIF (lihat pesan [API_CLIENT] saat boot)");
    return;
  }

  uint32_t newest = 0, count = 0, epoch = 0;
  uint32_t pending = 0;
  if (eventLogger.snapshot(newest, count, epoch)) {
    uint32_t marker = (epoch == _epoch) ? _lastSyncedSequence : 0;
    pending = (newest > marker) ? (newest - marker) : 0;
  }

  Serial.printf("Endpoint  : %s\n", BACKEND_EVENTS_ENDPOINT);
  Serial.printf("Device ID : %s\n", DEVICE_ID);
  Serial.printf("Menunggu  : %lu event belum terkirim (penanda=#%lu, epoch=%lu)\n",
                (unsigned long)pending, (unsigned long)_lastSyncedSequence, (unsigned long)_epoch);
  Serial.printf("Hasil akhir: %s (HTTP %d), total terkirim=%lu\n",
                syncResultToString(_lastResult), _lastHttpCode, (unsigned long)_totalSent);
  if (_everSucceeded) {
    Serial.printf("Sukses terakhir: %lu s lalu\n", (millis() - _lastSuccessAt) / 1000UL);
  }
  if (_failCount > 0) {
    long wait = static_cast<long>(_nextAttemptAt - millis());
    Serial.printf("Gagal beruntun: %u, retry berikutnya ~%ld s\n", (unsigned)_failCount, wait > 0 ? wait / 1000L : 0L);
  }
  Serial.printf("Network task: tick terakhir %lu ms lalu\n", millis() - _heartbeatAt);
}
