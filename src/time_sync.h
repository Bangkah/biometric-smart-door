#pragma once
/*
 * time_sync.h
 * ------------------------------------------------------------
 * NTP Time Synchronization (Phase 3).
 *
 * SNTP client bawaan ESP32 berjalan asinkron di latar belakang setelah
 * configTime(); modul ini hanya MEM-POLLING apakah waktu sistem sudah
 * masuk akal (>= MIN_VALID_UNIX_TIME). Tidak ada penantian blocking.
 *
 * Setelah tersinkron, time(nullptr) = epoch UTC riil, dipakai event_logger
 * untuk mengisi EventRecord::unixTimestamp. Waktu tetap berjalan saat Wi-Fi
 * putus, tetapi HILANG saat reboot (tidak ada RTC baterai) -> event pada
 * boot baru dicatat unixTimestamp=0 sampai NTP sinkron lagi; api_client
 * mengestimasi timestamp-nya saat upload (lihat estimateUnixForUptime).
 * ------------------------------------------------------------
 */

#include <Arduino.h>
#include <time.h>

class TimeSync {
  public:
    void begin();   // reset state; SNTP baru dimulai setelah Wi-Fi terhubung
    void update();  // tick non-blocking (dari network task)

    bool isSynced() const { return _synced; }

    // Epoch UTC (detik). 0 jika belum tersinkron.
    time_t nowUnix() const;

    // Perkiraan epoch UTC untuk event yang dicatat pada millis() tertentu
    // DI SESI BOOT INI. 0 jika belum sinkron.
    uint32_t estimateUnixForUptime(uint32_t eventMillis) const;

    // "YYYY-MM-DD HH:MM:SS (UTC+0700)" atau "belum tersinkron (NTP)".
    String getFormattedTime() const;

  private:
    volatile bool _synced = false;
    bool _started = false;
    unsigned long _lastAttemptAt = 0;

    void startSntp();
};

extern TimeSync timeSync;
