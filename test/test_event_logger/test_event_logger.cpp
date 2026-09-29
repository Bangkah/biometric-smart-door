// Unit test untuk event_logger.cpp PRODUKSI SESUNGGUHNYA (bukan tiruan) —
// dikompilasi bersama test/common_stubs.cpp yang menyediakan LittleFS,
// Preferences, dan FreeRTOS palsu TAPI FUNGSIONAL (in-memory, persisten
// antar instance EventLogger dalam satu proses -> mensimulasikan reboot
// ESP32 tanpa mem-flash hardware sungguhan).
//
// Ini adalah test PALING PENTING di Phase 3: memverifikasi klaim inti
// Phase 2 (SRS 5.6, 13.2) — ring buffer, ketahanan power-loss, dan
// epoch/generasi log — benar-benar berperilaku seperti didesain.
#include <unity.h>
#include <string>
#include "config.h"       // EVENT_LOG_MAX_ENTRIES, dst.
#include "event_logger.h"
#include "LittleFS.h"
#include "Preferences.h"

static uint32_t g_fixedUnixTime = 0;
static uint32_t fixedTimeProvider() { return g_fixedUnixTime; }
static uint32_t zeroTimeProvider() { return 0; }

void setUp() {
  // "Flash" & "NVS" kosong di awal SETIAP test case (test case lain yang
  // ingin mensimulasikan reboot melakukannya dengan membuat instance
  // EventLogger baru TANPA memanggil reset ini di tengah test).
  __test_reset_fake_fs();
  __test_reset_fake_nvs();
  g_fixedUnixTime = 0;
}
void tearDown() {}

// ------------------------------------------------------------
static void test_fresh_boot_is_empty_and_bumps_epoch_to_1() {
  EventLogger logger;
  TEST_ASSERT_TRUE(logger.begin());
  TEST_ASSERT_EQUAL_UINT32(0, logger.count());
  TEST_ASSERT_EQUAL_UINT16(1, logger.bootId());

  uint32_t newest = 99, count = 99, epoch = 99;
  TEST_ASSERT_TRUE(logger.snapshot(newest, count, epoch));
  TEST_ASSERT_EQUAL_UINT32(0, newest);
  TEST_ASSERT_EQUAL_UINT32(0, count);
  // File ring buffer baru dibuat -> bumpEpoch() otomatis (0 -> 1).
  TEST_ASSERT_EQUAL_UINT32(1, epoch);
}

// ------------------------------------------------------------
static void test_log_event_persists_fields_correctly() {
  EventLogger logger;
  TEST_ASSERT_TRUE(logger.begin());

  logger.logEvent(EventType::SYSTEM_BOOT, -1, "Firmware boot");
  logger.logEvent(EventType::ACCESS_GRANTED, 7, "Fingerprint matched");
  logger.logEvent(EventType::ACCESS_DENIED, -1, "Fingerprint not recognized");

  uint32_t newest = 0, count = 0, epoch = 0;
  TEST_ASSERT_TRUE(logger.snapshot(newest, count, epoch));
  TEST_ASSERT_EQUAL_UINT32(3, newest);
  TEST_ASSERT_EQUAL_UINT32(3, count);

  EventRecord recs[10];
  uint32_t n = logger.collectAfter(0, recs, 10);
  TEST_ASSERT_EQUAL_UINT32(3, n);

  TEST_ASSERT_EQUAL_UINT32(1, recs[0].sequence);
  TEST_ASSERT_EQUAL_UINT8((uint8_t)EventType::SYSTEM_BOOT, recs[0].type);

  TEST_ASSERT_EQUAL_UINT32(2, recs[1].sequence);
  TEST_ASSERT_EQUAL_INT16(7, recs[1].detail);
  TEST_ASSERT_EQUAL_UINT8((uint8_t)EventType::ACCESS_GRANTED, recs[1].type);
  TEST_ASSERT_EQUAL_STRING("Fingerprint matched", recs[1].message);

  TEST_ASSERT_EQUAL_UINT32(3, recs[2].sequence);
  TEST_ASSERT_EQUAL_UINT8((uint8_t)EventType::ACCESS_DENIED, recs[2].type);
}

// ------------------------------------------------------------
static void test_collect_after_filters_by_sequence() {
  EventLogger logger;
  TEST_ASSERT_TRUE(logger.begin());
  logger.logEvent(EventType::SYSTEM_BOOT, -1, "a");
  logger.logEvent(EventType::ACCESS_GRANTED, -1, "b");
  logger.logEvent(EventType::ACCESS_DENIED, -1, "c");

  EventRecord recs[10];
  uint32_t n = logger.collectAfter(1, recs, 10);  // sudah tersinkron s/d #1
  TEST_ASSERT_EQUAL_UINT32(2, n);
  TEST_ASSERT_EQUAL_UINT32(2, recs[0].sequence);
  TEST_ASSERT_EQUAL_UINT32(3, recs[1].sequence);
}

// ------------------------------------------------------------
// INI TEST KETAHANAN POWER-LOSS: instance EventLogger BARU dibuat tanpa
// mereset fake fs/nvs, mensimulasikan ESP32 reboot dengan flash & NVS
// yang selamat (persis skenario uji #5 di README bagian 8.4).
// ------------------------------------------------------------
static void test_events_survive_reboot() {
  {
    EventLogger before;
    TEST_ASSERT_TRUE(before.begin());
    before.logEvent(EventType::SYSTEM_BOOT, -1, "sesi 1");
    before.logEvent(EventType::ACCESS_GRANTED, 1, "sesi 1");
    before.logEvent(EventType::ACCESS_DENIED, -1, "sesi 1");
  }  // "power loss" -> objek hilang, TAPI __test_fake_fs()/__test_fake_nvs() tetap ada

  EventLogger after;
  TEST_ASSERT_TRUE(after.begin());
  TEST_ASSERT_EQUAL_UINT16(2, after.bootId());  // bootCount di NVS naik -> reboot ke-2

  uint32_t newest = 0, count = 0, epoch = 0;
  TEST_ASSERT_TRUE(after.snapshot(newest, count, epoch));
  TEST_ASSERT_EQUAL_UINT32(3, newest);  // 3 event sesi lalu TIDAK hilang
  TEST_ASSERT_EQUAL_UINT32(3, count);
  TEST_ASSERT_EQUAL_UINT32(1, epoch);   // reboot biasa TIDAK mengubah epoch

  // Event baru di sesi ini melanjutkan sequence, bukan mulai dari 1 lagi.
  after.logEvent(EventType::SYSTEM_BOOT, -1, "sesi 2");
  TEST_ASSERT_TRUE(after.snapshot(newest, count, epoch));
  TEST_ASSERT_EQUAL_UINT32(4, newest);
  TEST_ASSERT_EQUAL_UINT32(4, count);
}

// ------------------------------------------------------------
static void test_clear_all_bumps_epoch_and_resets_sequence() {
  EventLogger logger;
  TEST_ASSERT_TRUE(logger.begin());
  logger.logEvent(EventType::SYSTEM_BOOT, -1, "a");
  logger.logEvent(EventType::ACCESS_GRANTED, -1, "b");

  logger.clearAll();

  uint32_t newest = 0, count = 0, epoch = 0;
  TEST_ASSERT_TRUE(logger.snapshot(newest, count, epoch));
  // clearAll() mencatat SATU event LOG_CLEARED sebagai jejak audit (SRS 12.7).
  TEST_ASSERT_EQUAL_UINT32(1, count);
  TEST_ASSERT_EQUAL_UINT32(1, newest);   // sequence mulai dari 1 lagi
  TEST_ASSERT_EQUAL_UINT32(2, epoch);    // epoch naik: 1 -> 2

  EventRecord recs[5];
  uint32_t n = logger.collectAfter(0, recs, 5);
  TEST_ASSERT_EQUAL_UINT32(1, n);
  TEST_ASSERT_EQUAL_UINT8((uint8_t)EventType::LOG_CLEARED, recs[0].type);
}

// ------------------------------------------------------------
// PENTING untuk ApiClient (Phase 3): tanpa epoch yang persisten, penanda
// lastSyncedSequence lama akan "cocok" dengan sequence baru pasca-clear
// (sama-sama mulai dari 1) dan event baru TIDAK PERNAH terkirim ke backend.
// ------------------------------------------------------------
static void test_epoch_persists_across_reboot_after_clear() {
  {
    EventLogger before;
    TEST_ASSERT_TRUE(before.begin());
    before.logEvent(EventType::SYSTEM_BOOT, -1, "a");
    before.clearAll();  // epoch: 1 -> 2
  }

  EventLogger after;
  TEST_ASSERT_TRUE(after.begin());
  uint32_t newest = 0, count = 0, epoch = 0;
  TEST_ASSERT_TRUE(after.snapshot(newest, count, epoch));
  TEST_ASSERT_EQUAL_UINT32(2, epoch);  // dibaca dari NVS, bukan default 0/1
  TEST_ASSERT_EQUAL_UINT32(1, count);  // event LOG_CLEARED dari sesi lalu tetap ada
}

// ------------------------------------------------------------
static void test_ring_buffer_wraps_at_capacity_discarding_oldest() {
  EventLogger logger;
  TEST_ASSERT_TRUE(logger.begin());

  const uint32_t CAP = EVENT_LOG_MAX_ENTRIES;
  for (uint32_t i = 0; i < CAP + 5; i++) {
    logger.logEvent(EventType::ACCESS_GRANTED, (int16_t)i, "x");
  }

  uint32_t newest = 0, count = 0, epoch = 0;
  TEST_ASSERT_TRUE(logger.snapshot(newest, count, epoch));
  TEST_ASSERT_EQUAL_UINT32(CAP + 5, newest);
  TEST_ASSERT_EQUAL_UINT32(CAP, count);  // tidak pernah melebihi kapasitas

  EventRecord recs[EVENT_LOG_MAX_ENTRIES + 10];
  uint32_t n = logger.collectAfter(0, recs, EVENT_LOG_MAX_ENTRIES + 10);
  TEST_ASSERT_EQUAL_UINT32(CAP, n);
  // 5 event tertua (#1..#5) tertimpa; yang tersisa mulai dari #6, urut naik.
  TEST_ASSERT_EQUAL_UINT32(6, recs[0].sequence);
  TEST_ASSERT_EQUAL_UINT32(CAP + 5, recs[CAP - 1].sequence);
}

// ------------------------------------------------------------
static void test_unix_timestamp_zero_until_time_provider_synced() {
  EventLogger logger;
  logger.setTimeProvider(zeroTimeProvider);
  TEST_ASSERT_TRUE(logger.begin());

  logger.logEvent(EventType::SYSTEM_BOOT, -1, "sebelum sync");

  g_fixedUnixTime = 1735689700UL;
  logger.setTimeProvider(fixedTimeProvider);
  logger.logEvent(EventType::ACCESS_GRANTED, 1, "sesudah sync");

  EventRecord recs[5];
  uint32_t n = logger.collectAfter(0, recs, 5);
  TEST_ASSERT_EQUAL_UINT32(2, n);
  TEST_ASSERT_EQUAL_UINT32(0, recs[0].unixTimestamp);
  TEST_ASSERT_EQUAL_UINT32(1735689700UL, recs[1].unixTimestamp);
}

// ------------------------------------------------------------
static void test_collect_after_respects_max_out_limit() {
  EventLogger logger;
  TEST_ASSERT_TRUE(logger.begin());
  for (int i = 0; i < 5; i++) logger.logEvent(EventType::ACCESS_GRANTED, i, "x");

  EventRecord recs[2];
  uint32_t n = logger.collectAfter(0, recs, 2);  // batas 2, walau ada 5
  TEST_ASSERT_EQUAL_UINT32(2, n);
  TEST_ASSERT_EQUAL_UINT32(1, recs[0].sequence);
  TEST_ASSERT_EQUAL_UINT32(2, recs[1].sequence);
}

// ------------------------------------------------------------
int main() {
  UNITY_BEGIN();
  RUN_TEST(test_fresh_boot_is_empty_and_bumps_epoch_to_1);
  RUN_TEST(test_log_event_persists_fields_correctly);
  RUN_TEST(test_collect_after_filters_by_sequence);
  RUN_TEST(test_events_survive_reboot);
  RUN_TEST(test_clear_all_bumps_epoch_and_resets_sequence);
  RUN_TEST(test_epoch_persists_across_reboot_after_clear);
  RUN_TEST(test_ring_buffer_wraps_at_capacity_discarding_oldest);
  RUN_TEST(test_unix_timestamp_zero_until_time_provider_synced);
  RUN_TEST(test_collect_after_respects_max_out_limit);
  return UNITY_END();
}
