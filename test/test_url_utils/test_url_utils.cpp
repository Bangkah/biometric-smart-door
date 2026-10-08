// Unit test untuk src/url_utils.h (Phase 3, header-only, murni).
// Dipakai api_client.cpp (heartbeat) DAN src/phase4/command_client.cpp
// (commands/poll, commands/ack) — lihat catatan batas fase di url_utils.h.
#include <unity.h>
#include "url_utils.h"

void setUp() {}
void tearDown() {}

static void test_derive_base_url_https() {
  char buf[64];
  TEST_ASSERT_TRUE(urlutils::deriveBaseUrl("https://backend.example.com/api/v1/events", buf, sizeof(buf)));
  TEST_ASSERT_EQUAL_STRING("https://backend.example.com", buf);
}

static void test_derive_base_url_with_port_and_bench_http() {
  char buf[64];
  TEST_ASSERT_TRUE(urlutils::deriveBaseUrl("http://192.168.1.50:8000/api/v1/events", buf, sizeof(buf)));
  TEST_ASSERT_EQUAL_STRING("http://192.168.1.50:8000", buf);
}

static void test_derive_base_url_rejects_wrong_suffix() {
  char buf[64];
  TEST_ASSERT_FALSE(urlutils::deriveBaseUrl("https://backend.example.com/api/v2/ingest", buf, sizeof(buf)));
}

static void test_derive_base_url_rejects_url_shorter_than_suffix() {
  char buf[64];
  TEST_ASSERT_FALSE(urlutils::deriveBaseUrl("/api/v1/events", buf, sizeof(buf)));
}

static void test_derive_base_url_buffer_too_small_fails_safely() {
  char buf[5];
  TEST_ASSERT_FALSE(urlutils::deriveBaseUrl("https://backend.example.com/api/v1/events", buf, sizeof(buf)));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_derive_base_url_https);
  RUN_TEST(test_derive_base_url_with_port_and_bench_http);
  RUN_TEST(test_derive_base_url_rejects_wrong_suffix);
  RUN_TEST(test_derive_base_url_rejects_url_shorter_than_suffix);
  RUN_TEST(test_derive_base_url_buffer_too_small_fails_safely);
  return UNITY_END();
}
