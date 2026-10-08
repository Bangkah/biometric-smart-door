#pragma once
/*
 * command_client.h
 * ------------------------------------------------------------
 * [PHASE 4] Remote command polling & execution (SRS 10.5, 10.6, 12.5).
 *
 * Alur:
 *   1. update() (network task) polling GET .../commands/poll secara berkala.
 *   2. Setiap command diverifikasi: HMAC signature (command_policy.h) DAN
 *      belum kadaluwarsa (isCommandValidNow, fail-closed jika NTP belum
 *      sinkron) — SEBELUM dieksekusi apa pun.
 *   3. ALLOW_SLOT/REVOKE_SLOT dieksekusi LANGSUNG di network task (hanya
 *      menulis NVS lewat access_policy, cepat & tanpa menyentuh aktuator).
 *   4. REMOTE_UNLOCK TIDAK dieksekusi di sini — hanya menitipkan permintaan
 *      lewat "bridge" ke state machine (loop task), yang SATU-SATUNYA
 *      pemilik aktuator/pintu. Ini menjaga invarian arsitektur sejak
 *      Phase 1: kontrol fisik pintu selalu di loop task, jaringan tidak
 *      pernah menyentuhnya langsung.
 *
 * Bridge (consumeUnlockRequest/reportUnlockResult) di-mutex sama seperti
 * pola event_logger — satu slot in-flight (satu remote-unlock per waktu;
 * command kedua yang datang saat masih pending langsung ditolak "busy").
 * Permintaan yang tidak kunjung dikonsumsi loop task (mis. device macet di
 * ERROR_SAFE) otomatis kadaluwarsa sendiri sesuai expires_at command,
 * dilaporkan "expired" ke backend — tidak pernah menggantung selamanya.
 * ------------------------------------------------------------
 */

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "config.h"  // COMMAND_ID_MAX_LEN dipakai langsung di ukuran array anggota di bawah

class CommandClient {
  public:
    void begin();
    void update();  // dipanggil dari network task

    bool isEnabled() const { return _enabled; }
    void printStatus() const;

    // ---- Jembatan ke state machine (dipanggil dari LOOP TASK) ----
    // Dipanggil dari StateMachine::runIdle(). Jika true, idOut berisi
    // command_id yang HARUS dieksekusi (buka kunci) SEKARANG; state machine
    // WAJIB memanggil reportUnlockResult() setelah selesai.
    bool consumeUnlockRequest(char* idOut, size_t idOutLen);
    void reportUnlockResult(const char* commandId, bool success, const char* detail);

    // Dipanggil dari fungsi bebas processCommand() di command_client.cpp
    // (anonymous namespace) lewat parameter `CommandClient& self` -- PUBLIC
    // (bukan private+friend) supaya tidak perlu forward-declare fungsi
    // anonymous-namespace itu lintas header/cpp. Tidak dipanggil dari luar
    // modul ini dalam praktiknya, tapi tidak ada risiko keamanan jika
    // terpanggil (hanya kirim HTTP ack / hitung HMAC).
    void ackCommand(const char* commandId, const char* status, const char* detail);
    bool hmacSha256Hex(const char* message, char* outHex, size_t outHexLen);

  private:
    bool _enabled = false;
    bool _https = false;
    String _pollUrl;
    String _ackUrlPrefix;

    unsigned long _nextPollAt = 0;
    uint8_t _failCount = 0;
    int _lastHttpCode = 0;
    unsigned long _lastSuccessAt = 0;
    bool _everSucceeded = false;
    uint32_t _totalExecuted = 0;
    uint32_t _totalRejected = 0;

    // ---- Bridge ke state machine (mutex-protected, 1 slot in-flight) ----
    SemaphoreHandle_t _bridgeMutex = nullptr;
    volatile bool _unlockPending = false;
    char _unlockCommandId[COMMAND_ID_MAX_LEN] = {0};
    uint32_t _unlockExpiresAt = 0;  // epoch unix, untuk auto-expire di net task
    volatile bool _unlockResultReady = false;
    bool _unlockResultSuccess = false;
    char _unlockResultCommandId[COMMAND_ID_MAX_LEN] = {0};
    char _unlockResultDetail[64] = {0};

    void pollOnce();
    // Penanganan per-command (parse field, verifikasi HMAC, eksekusi) hidup
    // sebagai fungsi bebas di command_client.cpp (menerima ArduinoJson
    // JsonObjectConst) -- TIDAK dideklarasikan sebagai method di sini, supaya
    // header ini tidak perlu #include <ArduinoJson.h> (header berat) sama
    // sekali. pollOnce() memanggilnya langsung dari dalam .cpp yang sama.
    // (ackCommand/hmacSha256Hex yang dipanggilnya sudah dideklarasikan PUBLIC
    // di atas -- lihat komentar di sana; TIDAK diulang di sini supaya tidak
    // redeclare member yang sama di dua access-specifier berbeda.)
    void scheduleRetry(unsigned long now);
    void checkUnlockBridgeExpiry();
    void drainUnlockResultIfReady();
};

extern CommandClient commandClient;
