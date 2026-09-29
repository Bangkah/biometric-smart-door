#pragma once
/*
 * backoff.h
 * ------------------------------------------------------------
 * Exponential backoff murni (tanpa dependensi Arduino/ESP-IDF),
 * dipakai bersama oleh NetworkManager (reconnect Wi-Fi) dan
 * ApiClient (retry upload). Diekstrak jadi fungsi terpisah agar:
 *   1. Kedua modul punya perilaku backoff yang identik & konsisten.
 *   2. Bisa diuji langsung via native unit test (test/test_backoff),
 *      tanpa perlu mem-flash ESP32 atau mensimulasikan Wi-Fi/HTTP.
 * ------------------------------------------------------------
 */

#include <stdint.h>

// delay = base * 2^min(attempt,6), dibatasi maxDelay.
// attempt=0 -> base (percobaan pertama tidak perlu menunggu ekstra).
inline unsigned long computeBackoffDelay(uint32_t attempt, unsigned long base, unsigned long maxDelay) {
  uint32_t exponent = (attempt > 6) ? 6 : attempt;  // cap agar (1UL << exponent) tidak overflow
  unsigned long delay = base * (1UL << exponent);
  return (delay > maxDelay || delay < base) ? maxDelay : delay;  // delay<base menandakan overflow
}
