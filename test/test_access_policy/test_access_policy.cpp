// Unit test untuk access_policy.cpp PRODUKSI SESUNGGUHNYA (Phase 4),
// dikompilasi bersama test/stubs/Preferences.h yang FUNGSIONAL (in-memory,
// persisten antar instance -> mensimulasikan reboot).
#include <unity.h>
#include <cstring>
#include "config.h"
#include "access_policy.h"
#include "Preferences.h"

void setUp() { __test_reset_fake_nvs(); }
void tearDown() {}

static void test_default_all_slots_allowed_on_fresh_boot() {
  // Rasional: device yang upgrade dari firmware Phase 1-3 (belum pernah ada
  // command ALLOW/REVOKE) TIDAK BOLEH tiba-tiba mengunci semua orang.
  AccessPolicy policy;
  TEST_ASSERT_TRUE(policy.begin());
  TEST_ASSERT_TRUE(policy.isSlotAllowed(0));
  TEST_ASSERT_TRUE(policy.isSlotAllowed(1));
  TEST_ASSERT_TRUE(policy.isSlotAllowed(AccessPolicy::kMaxSlot));
}

static void test_revoke_blocks_only_that_slot() {
  AccessPolicy policy;
  policy.begin();
  policy.setSlotAllowed(1, false);

  TEST_ASSERT_FALSE(policy.isSlotAllowed(1));
  TEST_ASSERT_TRUE(policy.isSlotAllowed(0));
  TEST_ASSERT_TRUE(policy.isSlotAllowed(2));
}

static void test_allow_after_revoke_restores_access() {
  AccessPolicy policy;
  policy.begin();
  policy.setSlotAllowed(5, false);
  TEST_ASSERT_FALSE(policy.isSlotAllowed(5));

  policy.setSlotAllowed(5, true);
  TEST_ASSERT_TRUE(policy.isSlotAllowed(5));
}

static void test_out_of_range_slot_is_fail_closed() {
  AccessPolicy policy;
  policy.begin();
  TEST_ASSERT_FALSE(policy.isSlotAllowed(-1));
  TEST_ASSERT_FALSE(policy.isSlotAllowed(64));
  TEST_ASSERT_FALSE(policy.isSlotAllowed(1000));
}

static void test_set_out_of_range_slot_is_noop_not_crash() {
  AccessPolicy policy;
  policy.begin();
  policy.setSlotAllowed(999, false);  // tidak boleh crash / corrupt mask
  TEST_ASSERT_EQUAL_UINT32(0xFFFFFFFFu, (uint32_t)(policy.snapshotMask() & 0xFFFFFFFFu));
}

// ---- Ketahanan power-loss / persistensi lintas reboot ----
static void test_revocation_survives_reboot() {
  {
    AccessPolicy before;
    before.begin();
    before.setSlotAllowed(3, false);
    before.setSlotAllowed(7, false);
  }  // "power loss" -> objek hilang, fake NVS TIDAK direset (lihat setUp())

  AccessPolicy after;
  TEST_ASSERT_TRUE(after.begin());
  TEST_ASSERT_FALSE(after.isSlotAllowed(3));
  TEST_ASSERT_FALSE(after.isSlotAllowed(7));
  TEST_ASSERT_TRUE(after.isSlotAllowed(1));  // slot lain tidak ikut ter-revoke
}

static void test_mask_independent_from_event_logger_nvs_namespace() {
  // Sanity check desain: access_policy pakai namespace NVS SENDIRI
  // (ACCESS_POLICY_NVS_NAMESPACE != EVENT_LOG_NVS_NAMESPACE), supaya
  // penghapusan log (event_logger.clearAll()) tidak ikut mereset kebijakan akses.
  TEST_ASSERT_TRUE(strcmp(ACCESS_POLICY_NVS_NAMESPACE, EVENT_LOG_NVS_NAMESPACE) != 0);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_default_all_slots_allowed_on_fresh_boot);
  RUN_TEST(test_revoke_blocks_only_that_slot);
  RUN_TEST(test_allow_after_revoke_restores_access);
  RUN_TEST(test_out_of_range_slot_is_fail_closed);
  RUN_TEST(test_set_out_of_range_slot_is_noop_not_crash);
  RUN_TEST(test_revocation_survives_reboot);
  RUN_TEST(test_mask_independent_from_event_logger_nvs_namespace);
  return UNITY_END();
}
