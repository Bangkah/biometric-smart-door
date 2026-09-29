#include "network_manager.h"
#include "config.h"
#include "backoff.h"

#include <WiFi.h>
#include <string.h>

NetworkManager networkManager;

// Alasan disconnect terakhir dari event Wi-Fi (untuk diagnosis: password salah,
// AP tidak terjangkau, dst). Hanya disimpan; dicetak dari update().
static volatile uint8_t g_lastDisconnectReason = 0;

static void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    g_lastDisconnectReason = info.wifi_sta_disconnected.reason;
  }
}

static const char* reasonHint(uint8_t r) {
  switch (r) {
    case 2:   return "AUTH_EXPIRE";
    case 15:  return "4WAY_HANDSHAKE_TIMEOUT (password salah?)";
    case 200: return "BEACON_TIMEOUT";
    case 201: return "NO_AP_FOUND (SSID tidak terjangkau)";
    case 202: return "AUTH_FAIL (password salah?)";
    case 203: return "ASSOC_FAIL";
    case 204: return "HANDSHAKE_TIMEOUT";
    default:  return "";
  }
}

void NetworkManager::begin() {
  if (!NETWORK_ENABLED || strlen(WIFI_SSID) == 0) {
    _state = WiFiState::NotConfigured;
    Serial.println("[NETWORK] WIFI_SSID kosong / jaringan nonaktif -> mode OFFLINE "
                   "(perilaku Phase 1/2). Salin include/secrets.h.example -> secrets.h untuk mengaktifkan.");
    return;
  }

  WiFi.persistent(false);          // hindari wear flash NVS bawaan Wi-Fi
  WiFi.setHostname(DEVICE_ID);
  WiFi.mode(WIFI_STA);
  // Reconnect dikelola sendiri (update()) agar backoff & logging terkontrol,
  // bukan diserahkan ke auto-reconnect bawaan firmware Wi-Fi.
  WiFi.setAutoReconnect(false);
  WiFi.onEvent(onWiFiEvent);

  Serial.printf("[NETWORK] Menghubungkan ke SSID \"%s\"...\n", WIFI_SSID);
  startConnectAttempt();
}

void NetworkManager::startConnectAttempt() {
  WiFi.disconnect();  // pastikan bersih sebelum percobaan baru
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  _state = WiFiState::Connecting;
  _connectStartedAt = millis();
  _lastAttemptAt = millis();
}

unsigned long NetworkManager::currentBackoffDelay() const {
  return computeBackoffDelay(_reconnectAttempts, WIFI_BACKOFF_BASE_MS, WIFI_BACKOFF_MAX_MS);
}

void NetworkManager::update() {
  switch (_state) {
    case WiFiState::NotConfigured:
      break;

    case WiFiState::Connecting:
      if (WiFi.status() == WL_CONNECTED) {
        _state = WiFiState::Connected;
        _connectedSinceMs = millis();
        _reconnectAttempts = 0;
        Serial.printf("[NETWORK] Terhubung! IP: %s, RSSI: %d dBm\n",
                      WiFi.localIP().toString().c_str(), (int)WiFi.RSSI());
      } else if (millis() - _connectStartedAt > WIFI_CONNECT_TIMEOUT_MS) {
        _state = WiFiState::Disconnected;
        _reconnectAttempts++;
        _lastAttemptAt = millis();
        uint8_t reason = g_lastDisconnectReason;
        Serial.printf("[NETWORK] Timeout koneksi (alasan terakhir=%u %s). Retry dalam %lu ms.\n",
                      (unsigned)reason, reasonHint(reason), currentBackoffDelay());
      }
      break;

    case WiFiState::Connected:
      if (WiFi.status() != WL_CONNECTED) {
        uint8_t reason = g_lastDisconnectReason;
        Serial.printf("[NETWORK] Koneksi Wi-Fi terputus (alasan=%u %s). Reconnect...\n",
                      (unsigned)reason, reasonHint(reason));
        _state = WiFiState::Reconnecting;
        _lastAttemptAt = millis();
        _reconnectAttempts = 0;  // putus setelah sempat konek -> retry cepat dulu
      }
      break;

    case WiFiState::Disconnected:
    case WiFiState::Reconnecting:
      if (millis() - _lastAttemptAt >= currentBackoffDelay()) {
        Serial.printf("[NETWORK] Mencoba koneksi (percobaan ke-%lu)...\n",
                      (unsigned long)(_reconnectAttempts + 1));
        startConnectAttempt();
      }
      break;
  }
}

String NetworkManager::getStatusString() const {
  switch (_state) {
    case WiFiState::NotConfigured: return "NOT_CONFIGURED (offline)";
    case WiFiState::Connected:     return "CONNECTED";
    case WiFiState::Connecting:    return "CONNECTING";
    case WiFiState::Reconnecting:  return "RECONNECTING";
    case WiFiState::Disconnected:
    default:                       return "DISCONNECTED";
  }
}

String NetworkManager::getLocalIP() const {
  if (_state == WiFiState::Connected) return WiFi.localIP().toString();
  return "0.0.0.0";
}

void NetworkManager::printStatus() const {
  Serial.println("---------- WI-FI ----------");
  Serial.printf("State     : %s\n", getStatusString().c_str());
  if (_state == WiFiState::NotConfigured) {
    Serial.println("(isi include/secrets.h untuk mengaktifkan jaringan)");
  } else {
    // Password & token SENGAJA tidak pernah dicetak.
    Serial.printf("SSID      : %s\n", WIFI_SSID);
    Serial.printf("IP        : %s\n", getLocalIP().c_str());
    if (_state == WiFiState::Connected) {
      Serial.printf("RSSI      : %d dBm\n", (int)WiFi.RSSI());
      Serial.printf("Uptime    : %lu s sejak tersambung\n", (millis() - _connectedSinceMs) / 1000UL);
    } else {
      uint8_t reason = g_lastDisconnectReason;
      Serial.printf("Retry #   : %lu (alasan terakhir=%u %s)\n",
                    (unsigned long)_reconnectAttempts, (unsigned)reason, reasonHint(reason));
    }
  }
}
