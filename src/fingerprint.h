#pragma once
/*
 * fingerprint.h
 * ------------------------------------------------------------
 * Abstraksi sensor sidik jari (SRS 4.2, 5.2).
 *
 * Dua mode kompilasi (dipilih di config.h):
 *  - MOCK_FINGERPRINT_MODE aktif: simulasi via Serial Monitor
 *    ('m' = match, 'x' = no match) dan/atau push button
 *    (short press = match, long press = no match). Dipakai untuk
 *    bench-testing state machine sebelum sensor fisik terpasang.
 *  - MOCK_FINGERPRINT_MODE nonaktif: driver asli untuk AS608/R307
 *    memakai Adafruit Fingerprint Sensor Library via UART2.
 *
 * Kontrak privasi (SRS 2.2 Privacy by Design, 12.6): modul ini
 * TIDAK PERNAH mengekspos citra sidik jari mentah. Ia hanya
 * mengembalikan hasil pencocokan (match/no-match) dan, jika
 * cocok, ID slot template lokal.
 * ------------------------------------------------------------
 */

#include <Arduino.h>

enum class FingerprintResult {
  NO_FINGER,     // Tidak ada sentuhan terdeteksi
  MATCH,         // Sidik jari cocok dengan template lokal
  NO_MATCH,      // Sidik jari terbaca tapi tidak cocok (DENIED)
  SENSOR_ERROR   // Kegagalan komunikasi/pembacaan sensor
};

class FingerprintModule {
  public:
    void begin();

    // Dipanggil saat state HARDWARE_CHECK. Mengembalikan false
    // jika sensor tidak merespons / tidak siap.
    bool healthCheck();

    // Polling non-blocking: apakah ada jari terdeteksi di sensor?
    // Dipanggil terus-menerus selama state IDLE.
    bool isFingerPresent();

    // Melakukan capture + matching. Dipanggil sekali saat masuk
    // state VERIFYING. Menghormati FP_VERIFY_TIMEOUT_MS (FR-001).
    FingerprintResult verify();

    // Jika verify() == MATCH, slot template yang cocok (untuk logging
    // lokal). -1 jika tidak relevan.
    int lastMatchedSlotId() const { return _lastMatchedSlot; }

  private:
    bool _sensorReady = false;
    int _lastMatchedSlot = -1;
};

extern FingerprintModule fingerprintModule;
