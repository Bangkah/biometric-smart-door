#include "time_sync.h"
#include "config.h"
#include "network_manager.h"
#include "time_estimate.h"

TimeSync timeSync;

void TimeSync::begin() {
  _synced = false;
  _started = false;
  Serial.println("[TIME_SYNC] Siap. SNTP dimulai otomatis setelah Wi-Fi terhubung.");
}

void TimeSync::startSntp() {
  configTime(NTP_GMT_OFFSET_SEC, NTP_DAYLIGHT_OFFSET_SEC, NTP_SERVER_1, NTP_SERVER_2, NTP_SERVER_3);
  _started = true;
  _lastAttemptAt = millis();
  Serial.printf("[TIME_SYNC] NTP dikonfigurasi (%s, %s, %s).\n", NTP_SERVER_1, NTP_SERVER_2, NTP_SERVER_3);
}

void TimeSync::update() {
  if (_synced) return;                       // setelah sinkron, SNTP menyegarkan diri sendiri
  if (!networkManager.isConnected()) return;  // tanpa jaringan tidak ada yang bisa dilakukan

  if (!_started) {
    startSntp();
    return;
  }

  if (time(nullptr) >= static_cast<time_t>(MIN_VALID_UNIX_TIME)) {
    _synced = true;
    Serial.print("[TIME_SYNC] Waktu tersinkron: ");
    Serial.println(getFormattedTime());
    return;
  }

  if (millis() - _lastAttemptAt >= NTP_RETRY_INTERVAL_MS) {
    Serial.println("[TIME_SYNC] Belum ada respons NTP, mengulang konfigurasi...");
    startSntp();
  }
}

time_t TimeSync::nowUnix() const {
  if (!_synced) return 0;
  time_t t = time(nullptr);
  return (t >= static_cast<time_t>(MIN_VALID_UNIX_TIME)) ? t : 0;
}

uint32_t TimeSync::estimateUnixForUptime(uint32_t eventMillis) const {
  return ::estimateUnixForUptime(static_cast<uint32_t>(nowUnix()), millis(), eventMillis);
}

String TimeSync::getFormattedTime() const {
  if (!_synced) return "belum tersinkron (NTP)";

  time_t now = time(nullptr);
  struct tm tmInfo;
  localtime_r(&now, &tmInfo);

  char buf[40];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S (UTC%z)", &tmInfo);
  return String(buf);
}
