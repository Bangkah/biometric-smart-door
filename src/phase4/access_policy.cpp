#include "access_policy.h"
#include "config.h"

#include <Preferences.h>

AccessPolicy accessPolicy;

bool AccessPolicy::begin() {
  Preferences prefs;
  prefs.begin(ACCESS_POLICY_NVS_NAMESPACE, false);
  // Default ~0ULL (semua slot diizinkan) bila key belum pernah ditulis --
  // lihat rasionalnya di access_policy.h.
  _mask = prefs.getULong64("mask", ~0ULL);
  prefs.end();

  Serial.printf("[ACCESS_POLICY] Siap. mask=0x%016llX (bit=1 berarti slot diizinkan)\n",
                static_cast<unsigned long long>(_mask));
  return true;
}

bool AccessPolicy::isSlotAllowed(int slot) const {
  if (slot < 0 || slot > kMaxSlot) return false;  // di luar jangkauan -> fail-closed
  return (_mask & (1ULL << slot)) != 0;
}

void AccessPolicy::setSlotAllowed(int slot, bool allowed) {
  if (slot < 0 || slot > kMaxSlot) {
    Serial.printf("[ACCESS_POLICY] slot %d di luar jangkauan (0-%d), diabaikan\n", slot, kMaxSlot);
    return;
  }

  if (allowed) {
    _mask |= (1ULL << slot);
  } else {
    _mask &= ~(1ULL << slot);
  }
  persist();

  Serial.printf("[ACCESS_POLICY] slot %d -> %s (mask baru=0x%016llX)\n", slot,
                allowed ? "ALLOWED" : "REVOKED", static_cast<unsigned long long>(_mask));
}

void AccessPolicy::persist() const {
  Preferences prefs;
  prefs.begin(ACCESS_POLICY_NVS_NAMESPACE, false);
  prefs.putULong64("mask", _mask);
  prefs.end();
}
