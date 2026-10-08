// Unit test untuk command_policy.h (logika murni, tanpa Arduino/mbedtls).
// Beberapa vektor uji SENGAJA identik dengan
// backend/tests/test_security.py agar kedua sisi (Python & C++) saling
// silang-verifikasi bahwa mereka membangun string yang SAMA PERSIS untuk
// input yang sama (prasyarat mutlak supaya HMAC di kedua sisi cocok).
#include <unity.h>
#include <cstring>
#include "command_policy.h"

void setUp() {}
void tearDown() {}

// ---------------- canonicalPayload ----------------
static void test_canonical_payload_remote_unlock_is_empty() {
  char buf[32] = "x";
  TEST_ASSERT_TRUE(commandpolicy::canonicalPayload("REMOTE_UNLOCK", false, 0, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL_STRING("", buf);
}

static void test_canonical_payload_allow_slot() {
  char buf[32];
  TEST_ASSERT_TRUE(commandpolicy::canonicalPayload("ALLOW_SLOT", true, 7, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL_STRING("slot=7", buf);  // identik dgn test_security.py:test_canonical_payload_allow_slot
}

static void test_canonical_payload_revoke_slot_zero() {
  char buf[32];
  TEST_ASSERT_TRUE(commandpolicy::canonicalPayload("REVOKE_SLOT", true, 0, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL_STRING("slot=0", buf);
}

static void test_canonical_payload_rejects_missing_slot() {
  char buf[32];
  TEST_ASSERT_FALSE(commandpolicy::canonicalPayload("ALLOW_SLOT", false, 0, buf, sizeof(buf)));
}

static void test_canonical_payload_rejects_unknown_type() {
  char buf[32];
  TEST_ASSERT_FALSE(commandpolicy::canonicalPayload("SELF_DESTRUCT", false, 0, buf, sizeof(buf)));
}

static void test_canonical_payload_buffer_too_small_fails_safely() {
  char buf[4];  // "slot=7" perlu 7 byte -> tidak muat
  TEST_ASSERT_FALSE(commandpolicy::canonicalPayload("ALLOW_SLOT", true, 7, buf, sizeof(buf)));
}

// ---------------- buildSignatureString ----------------
static void test_signature_string_format_matches_backend() {
  // Vektor identik dengan backend/tests/test_security.py:test_signature_string_format
  char buf[80];
  TEST_ASSERT_TRUE(commandpolicy::buildSignatureString("abc-123", "ALLOW_SLOT", 1000, 1060, "slot=7", buf, sizeof(buf)));
  TEST_ASSERT_EQUAL_STRING("abc-123.ALLOW_SLOT.1000.1060.slot=7", buf);
}

static void test_signature_string_remote_unlock_empty_payload() {
  char buf[80];
  TEST_ASSERT_TRUE(commandpolicy::buildSignatureString("id1", "REMOTE_UNLOCK", 1000, 1060, "", buf, sizeof(buf)));
  TEST_ASSERT_EQUAL_STRING("id1.REMOTE_UNLOCK.1000.1060.", buf);
}

static void test_signature_string_buffer_too_small_fails_safely() {
  char buf[10];
  TEST_ASSERT_FALSE(commandpolicy::buildSignatureString("abc-123", "ALLOW_SLOT", 1000, 1060, "slot=7", buf, sizeof(buf)));
}

// ---------------- isCommandValidNow ----------------
static void test_command_valid_when_not_yet_expired() {
  TEST_ASSERT_TRUE(commandpolicy::isCommandValidNow(1000, 1060));
}

static void test_command_invalid_when_expired() {
  TEST_ASSERT_FALSE(commandpolicy::isCommandValidNow(1061, 1060));
  TEST_ASSERT_FALSE(commandpolicy::isCommandValidNow(1060, 1060));  // batas: now==expires -> tidak valid lagi
}

static void test_command_invalid_when_time_unknown() {
  // Fail-closed: NTP belum sinkron (nowUnix==0) -> JANGAN pernah eksekusi,
  // walau expiresAt tampak jauh di masa depan.
  TEST_ASSERT_FALSE(commandpolicy::isCommandValidNow(0, 999999999));
}

// ---------------- constantTimeHexEqual ----------------
static void test_hex_equal_case_insensitive() {
  TEST_ASSERT_TRUE(commandpolicy::constantTimeHexEqual("aB12cd", "Ab12CD"));
}

static void test_hex_equal_rejects_mismatch() {
  TEST_ASSERT_FALSE(commandpolicy::constantTimeHexEqual("aabbcc", "aabbcd"));
}

static void test_hex_equal_rejects_different_length() {
  TEST_ASSERT_FALSE(commandpolicy::constantTimeHexEqual("aabb", "aabbcc"));
}

// NOTE: deriveBaseUrl() TIDAK ADA di sini — ia milik src/url_utils.h
// (Phase 3, dipakai api_client.cpp DAN command_client.cpp). Lihat
// test/test_url_utils/test_url_utils.cpp dan catatan batas fase di
// command_policy.h untuk rasionalnya.

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_canonical_payload_remote_unlock_is_empty);
  RUN_TEST(test_canonical_payload_allow_slot);
  RUN_TEST(test_canonical_payload_revoke_slot_zero);
  RUN_TEST(test_canonical_payload_rejects_missing_slot);
  RUN_TEST(test_canonical_payload_rejects_unknown_type);
  RUN_TEST(test_canonical_payload_buffer_too_small_fails_safely);
  RUN_TEST(test_signature_string_format_matches_backend);
  RUN_TEST(test_signature_string_remote_unlock_empty_payload);
  RUN_TEST(test_signature_string_buffer_too_small_fails_safely);
  RUN_TEST(test_command_valid_when_not_yet_expired);
  RUN_TEST(test_command_invalid_when_expired);
  RUN_TEST(test_command_invalid_when_time_unknown);
  RUN_TEST(test_hex_equal_case_insensitive);
  RUN_TEST(test_hex_equal_rejects_mismatch);
  RUN_TEST(test_hex_equal_rejects_different_length);
  return UNITY_END();
}
