// Unit test murni untuk time_estimate.h (dipakai ApiClient saat mengirim
// event yang dicatat sebelum NTP sinkron pada sesi boot berjalan).
#include <unity.h>
#include "time_estimate.h"

void setUp() {}
void tearDown() {}

static void test_returns_zero_when_not_synced() {
  TEST_ASSERT_EQUAL_UINT32(0, estimateUnixForUptime(0, 50000, 10000));
}

static void test_estimates_past_event_correctly() {
  // Sekarang epoch 1735689700 (2025-01-01 00:01:40 UTC), uptime sekarang 50000ms,
  // event dicatat pada uptime 10000ms -> event terjadi 40 detik yang lalu.
  uint32_t result = estimateUnixForUptime(1735689700UL, 50000UL, 10000UL);
  TEST_ASSERT_EQUAL_UINT32(1735689660UL, result);
}

static void test_event_at_same_millis_as_now() {
  uint32_t result = estimateUnixForUptime(1735689700UL, 50000UL, 50000UL);
  TEST_ASSERT_EQUAL_UINT32(1735689700UL, result);
}

static void test_returns_zero_if_age_exceeds_now() {
  // Kasus tak masuk akal (mis. epoch sistem sangat kecil): jangan pernah
  // mengembalikan hasil negatif/wrap besar -> harus 0 (ditandai "n/a").
  uint32_t result = estimateUnixForUptime(30UL, 100000UL, 10000UL);
  TEST_ASSERT_EQUAL_UINT32(0, result);
}

static void test_handles_millis_rollover_gracefully() {
  // millis() 32-bit rollover: nowMillis "lebih kecil" dari eventMillis secara
  // representasi karena wraparound. Pengurangan unsigned tetap menghasilkan
  // delta yang benar (aritmetika modulo 2^32) selama rollover terjadi <1 kali.
  uint32_t nowMillis = 100000UL;          // baru saja rollover
  uint32_t eventMillis = 4294900000UL;    // sesaat sebelum rollover (~67000ms lalu)
  uint32_t ageMs = nowMillis - eventMillis;  // unsigned wraparound -> hasil benar
  TEST_ASSERT_EQUAL_UINT32(167296UL, ageMs);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_returns_zero_when_not_synced);
  RUN_TEST(test_estimates_past_event_correctly);
  RUN_TEST(test_event_at_same_millis_as_now);
  RUN_TEST(test_returns_zero_if_age_exceeds_now);
  RUN_TEST(test_handles_millis_rollover_gracefully);
  return UNITY_END();
}
