#pragma once
/*
 * time_estimate.h
 * ------------------------------------------------------------
 * Estimasi epoch UTC untuk event lama yang dicatat SEBELUM NTP
 * sinkron pada sesi boot berjalan (unixTimestamp==0 saat dicatat),
 * berdasarkan selisih uptime terhadap waktu sekarang.
 *
 * Diekstrak murni (tanpa TimeSync/Arduino) agar bisa diuji langsung
 * lewat native unit test (test/test_time_estimate) tanpa memerlukan
 * jam sistem sungguhan atau koneksi NTP.
 * ------------------------------------------------------------
 */

#include <stdint.h>

// nowUnixSec  : epoch UTC saat ini (hasil TimeSync::nowUnix()), 0 = belum sinkron.
// nowMillis   : millis() saat ini.
// eventMillis : millis() saat event dicatat (harus berasal dari sesi boot yang SAMA
//               dengan nowMillis; lintas-reboot millis() tidak sebanding sama sekali).
// Return 0 bila belum sinkron atau bila hasil perhitungan tidak masuk akal
// (event "lebih baru dari sekarang" akibat overflow/rollover millis() 32-bit,
// yang terjadi setiap ~49.7 hari uptime).
inline uint32_t estimateUnixForUptime(uint32_t nowUnixSec, uint32_t nowMillis, uint32_t eventMillis) {
  if (nowUnixSec == 0) return 0;

  uint32_t ageMs = nowMillis - eventMillis;  // unsigned wraparound aman selama tidak melewati 1 siklus penuh
  uint32_t ageSec = ageMs / 1000UL;

  return (nowUnixSec > ageSec) ? (nowUnixSec - ageSec) : 0;
}
