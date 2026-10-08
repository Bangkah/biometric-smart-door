#pragma once
/*
 * api_client.h
 * ------------------------------------------------------------
<<<<<<< HEAD
 * Event Sync / Backend Upload (SRS 10.2 Events, FR-009, Phase 3) DAN
 * Device Heartbeat (SRS 8.2/10.3, FR-015, Phase 3 hardening).
=======
 * Event Sync / Backend Upload (SRS 10.2 Events, FR-009, Phase 3).
>>>>>>> origin/main
 *
 * Tanggung jawab (terpisah dari event_logger):
 *  - Melacak event mana yang SUDAH diterima backend: pasangan
 *    (log epoch, lastSyncedSequence) disimpan di NVS.
 *  - Mengambil batch event baru dari ring buffer (sequence > penanda),
 *    membungkus JSON, dan HTTP POST ke BACKEND_EVENTS_ENDPOINT.
<<<<<<< HEAD
 *  - Mengirim heartbeat periodik ke POST /api/v1/devices/heartbeat,
 *    SENGAJA independen dari siklus sync event di atas (lihat update()):
 *    heartbeat tetap jalan walau tidak ada event baru, dan sebaliknya
 *    kegagalan heartbeat TIDAK PERNAH menunda/membatalkan sync event.
 *    Endpoint heartbeat diturunkan otomatis dari BACKEND_EVENTS_ENDPOINT
 *    (urlutils::deriveBaseUrl, src/url_utils.h) supaya secrets.h Phase 3 yang sudah
 *    ada tetap kompatibel tanpa field baru.
 *
 * Non-blocking terhadap sistem: update() dipanggil dari NETWORK TASK.
 * HTTP/TLS boleh memblokir task itu (dibatasi timeout), TIDAK loop().
 * Kegagalan APA PUN di modul ini (sync maupun heartbeat) tidak pernah
 * mengubah state mesin akses lokal (SRS 13.6 Backend Failure).
 *
 * Kebijakan pengiriman event: at-least-once, tanpa kehilangan.
=======
 *
 * Non-blocking terhadap sistem: update() dipanggil dari NETWORK TASK.
 * HTTP/TLS boleh memblokir task itu (dibatasi timeout), TIDAK loop().
 *
 * Kebijakan pengiriman: at-least-once, tanpa kehilangan.
>>>>>>> origin/main
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

<<<<<<< HEAD
    // Tick dari network task: menjalankan heartbeat + sync berkala bila waktunya.
=======
    // Tick dari network task: menjalankan sync berkala / manual bila waktunya.
>>>>>>> origin/main
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
<<<<<<< HEAD
    volatile unsigned long _taskTickAt = 0;  // penanda diagnostik "network task masih hidup"

    // ---- Device Heartbeat (independen dari sync di atas) ----
    bool _heartbeatEnabled = false;
    String _heartbeatUrl;
    unsigned long _nextHeartbeatAt = 0;
    unsigned long _lastHeartbeatOkAt = 0;
    bool _everHeartbeatOk = false;
    uint16_t _heartbeatFailCount = 0;
    int _lastHeartbeatHttpCode = 0;
=======
    volatile unsigned long _heartbeatAt = 0;
>>>>>>> origin/main

    void loadState();
    void saveState();
    void syncOnce(bool manual);
<<<<<<< HEAD
    void sendHeartbeatIfDue(unsigned long now);
    int httpPost(const String& url, const uint8_t* body, size_t len, String &errInfo);
=======
    int httpPost(const uint8_t* body, size_t len, String &errInfo);
>>>>>>> origin/main
    void scheduleRetry(unsigned long now);
};

extern ApiClient apiClient;
