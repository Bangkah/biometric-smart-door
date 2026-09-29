// Unit test murni untuk backoff.h (dipakai NetworkManager & ApiClient).
// Tidak butuh stub Arduino/LittleFS sama sekali.
#include <unity.h>
#include "backoff.h"

void setUp() {}
void tearDown() {}

static void test_first_attempt_is_base_delay() {
  TEST_ASSERT_EQUAL_UINT32(2000UL, computeBackoffDelay(0, 2000UL, 60000UL));
}

static void test_delay_doubles_each_attempt() {
  TEST_ASSERT_EQUAL_UINT32(2000UL,  computeBackoffDelay(0, 2000UL, 60000UL));
  TEST_ASSERT_EQUAL_UINT32(4000UL,  computeBackoffDelay(1, 2000UL, 60000UL));
  TEST_ASSERT_EQUAL_UINT32(8000UL,  computeBackoffDelay(2, 2000UL, 60000UL));
  TEST_ASSERT_EQUAL_UINT32(16000UL, computeBackoffDelay(3, 2000UL, 60000UL));
}

static void test_delay_is_capped_at_max() {
  // 2000 * 2^5 = 64000, harus dipotong ke 60000 (WIFI_BACKOFF_MAX_MS di firmware).
  TEST_ASSERT_EQUAL_UINT32(60000UL, computeBackoffDelay(5, 2000UL, 60000UL));
  // Percobaan sangat banyak tetap harus di angka cap yang sama, tidak pernah melebihi.
  TEST_ASSERT_EQUAL_UINT32(60000UL, computeBackoffDelay(100, 2000UL, 60000UL));
}

static void test_exponent_capped_at_6_prevents_overflow() {
  // attempt jauh di atas 6 tidak boleh membuat shift overflow / delay balik jadi kecil.
  unsigned long huge = computeBackoffDelay(4000000000UL, 5000UL, 300000UL);
  TEST_ASSERT_EQUAL_UINT32(300000UL, huge);
}

static void test_matches_api_client_retry_policy_values() {
  // Nilai persis dari SYNC_RETRY_BASE_MS/SYNC_RETRY_MAX_MS di config.h Phase 3.
  TEST_ASSERT_EQUAL_UINT32(5000UL,   computeBackoffDelay(0, 5000UL, 300000UL));
  TEST_ASSERT_EQUAL_UINT32(300000UL, computeBackoffDelay(10, 5000UL, 300000UL));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_first_attempt_is_base_delay);
  RUN_TEST(test_delay_doubles_each_attempt);
  RUN_TEST(test_delay_is_capped_at_max);
  RUN_TEST(test_exponent_capped_at_6_prevents_overflow);
  RUN_TEST(test_matches_api_client_retry_policy_values);
  return UNITY_END();
}
