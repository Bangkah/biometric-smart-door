#pragma once
/*
 * network_manager.h
 * ------------------------------------------------------------
 * Wi-Fi Connection Manager (SRS 3.6 Communication Flow, Phase 3).
 *
 * Non-blocking: begin()/update() tidak pernah menunggu koneksi. Koneksi awal
 * dan reconnect dikelola sebagai state machine kecil dengan exponential
 * backoff (dibatasi WIFI_BACKOFF_MAX_MS) agar tidak membanjiri access point.
 *
 * update() dipanggil dari NETWORK TASK (lihat main.cpp), bukan dari loop(),
 * sehingga state machine utama tidak tersentuh sama sekali oleh urusan Wi-Fi.
 * Status dibaca dari task lain lewat field volatile (aman untuk 1 writer).
 * ------------------------------------------------------------
 */

#include <Arduino.h>

enum class WiFiState {
  NotConfigured,  // WIFI_SSID kosong / NETWORK_ENABLED=0 -> mode offline murni
  Disconnected,   // belum terhubung, menunggu jadwal retry berikutnya
  Connecting,     // WiFi.begin() sudah dipanggil, menunggu hasil
  Connected,      // terhubung dan punya IP
  Reconnecting    // sempat terhubung lalu putus, menunggu/mencoba lagi
};

class NetworkManager {
  public:
    // Memulai koneksi pertama. Non-blocking.
    void begin();

    // Tick non-blocking. Panggil berkala (dari network task).
    void update();

    bool isConnected() const { return _state == WiFiState::Connected; }
    WiFiState getState() const { return _state; }

    // Untuk perintah debugger Serial 'w'.
    String getStatusString() const;
    String getLocalIP() const;
    uint32_t getReconnectAttempts() const { return _reconnectAttempts; }
    void printStatus() const;

  private:
    volatile WiFiState _state = WiFiState::NotConfigured;
    unsigned long _connectStartedAt = 0;
    unsigned long _lastAttemptAt = 0;
    unsigned long _connectedSinceMs = 0;
    volatile uint32_t _reconnectAttempts = 0;

    void startConnectAttempt();
    unsigned long currentBackoffDelay() const;
};

extern NetworkManager networkManager;
