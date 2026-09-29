#pragma once
/*
 * api_client.h
 * ------------------------------------------------------------
 * Event Sync / Backend Upload (SRS 10.2 Events, FR-009, Phase 3).
 *
 * Tanggung jawab (terpisah dari event_logger):
 *  - Melacak event mana yang SUDAH diterima backend: pasangan
 *    (log epoch, lastSyncedSequence) disimpan di NVS.
 *  - Mengambil batch event baru dari ring buffer (sequence > penanda),
 *    membungkus JSON, dan HTTP POST ke BACKEND_EVENTS_ENDPOINT.
 *
 * Non-blocking terhadap sistem: update() dipanggil dari NETWORK TASK.
 * HTTP/TLS boleh memblokir task itu (dibatasi timeout), TIDAK loop().
 *
 * Kebijakan pengiriman: at-least-once, tanpa kehilangan.
 *  - Penanda maju HANYA setelah backend membalas 2xx (atau 409 = sudah ada).
 *  - Gagal (timeout/5xx/401/...) -> batch sama dicoba lagi dengan backoff
 *    eksponensial. Backend wajib idempoten: kunci unik
 *    (device_id, log_epoch, boot_id, sequence).
 * ------------------------------------------------------------
 */

#include <Arduino.h>

enum class SyncResult {
  NONE,             // belum pernah mencoba
  SUCCESS,          // batch terkirim & diterima
  NOTHING_TO_SYNC,  // tidak ada event baru
  NO_NETWORK,       // Wi-Fi/NTP belum siap
  HTTP_ERROR        // request gagal / ditolak backend
};

const char* syncResultToString(SyncResult result);

class ApiClient {
  public:
    // Memuat penanda dari NVS & memvalidasi konfigurasi (URL/TLS/token).
    void begin();

    // Tick dari network task: menjalankan sync berkala / manual bila waktunya.
    void update();

    // Perintah debugger 's': minta sync segera (dijalankan network task).
    void requestSyncNow() { _manualRequest = true; }

    bool isEnabled() const { return _enabled; }
    void printStatus() const;

  private:
    bool _enabled = false;
    bool _https = false;
    volatile bool _manualRequest = false;

    uint32_t _epoch = 0;
    uint32_t _lastSyncedSequence = 0;

    unsigned long _nextAttemptAt = 0;
    uint8_t _failCount = 0;
    int _lastHttpCode = 0;
    SyncResult _lastResult = SyncResult::NONE;
    unsigned long _lastSuccessAt = 0;
    bool _everSucceeded = false;
    uint32_t _totalSent = 0;
    volatile unsigned long _heartbeatAt = 0;

    void loadState();
    void saveState();
    void syncOnce(bool manual);
    int httpPost(const uint8_t* body, size_t len, String &errInfo);
    void scheduleRetry(unsigned long now);
};

extern ApiClient apiClient;
