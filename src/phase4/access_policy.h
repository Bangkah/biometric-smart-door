#pragma once
/*
 * access_policy.h
 * ------------------------------------------------------------
 * Kontrol akses per-slot sidik jari, disinkronkan dari backend lewat
 * command ALLOW_SLOT/REVOKE_SLOT (SRS 7.4 Revocation, FR-013, Phase 4).
 *
 * Slot yang di-*revoke* dari dashboard TIDAK LANGSUNG menghapus template
 * dari sensor fisik (AS608/R307 tidak diakses di sini) — mask ini adalah
 * lapisan otorisasi TAMBAHAN yang dicek SETELAH sensor menyatakan MATCH:
 * sidik jari boleh saja masih cocok secara fisik, tapi jika slot-nya
 * di-revoke, verify() akan tetap mengembalikan NO_MATCH (lihat
 * fingerprint.cpp). Ini konsisten dengan filosofi Local-First SRS 7.4:
 * penerapan pencabutan tidak butuh koneksi ke backend saat kejadian akses
 * berlangsung — cukup mask yang SUDAH tersinkron sebelumnya di NVS.
 *
 * Default (belum pernah ada command ALLOW/REVOKE sama sekali, mis. baru
 * upgrade dari firmware Phase 1-3): SEMUA slot diizinkan (mask=~0), supaya
 * device yang sudah berjalan di lapangan sejak Phase 1-3 tidak tiba-tiba
 * mengunci semua orang begitu firmware Phase 4 di-flash.
 * ------------------------------------------------------------
 */

#include <Arduino.h>

class AccessPolicy {
  public:
    static constexpr int kMaxSlot = 63;  // dibatasi lebar bitmask uint64_t

    bool begin();

    // false untuk slot di luar jangkauan [0, kMaxSlot] (fail-closed).
    bool isSlotAllowed(int slot) const;

    // Tidak berlaku (diabaikan, dengan log peringatan) untuk slot di luar
    // jangkauan. Langsung menulis ke NVS supaya bertahan dari reboot/power-loss.
    void setSlotAllowed(int slot, bool allowed);

    uint64_t snapshotMask() const { return _mask; }

  private:
    uint64_t _mask = ~0ULL;

    void persist() const;
};

extern AccessPolicy accessPolicy;
