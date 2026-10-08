#pragma once
/*
 * command_policy.h
 * ------------------------------------------------------------
 * Logika MURNI (tanpa Arduino/mbedtls/HTTP) untuk memverifikasi command
 * jarak jauh (SRS 10.5, 10.6, 12.5, Phase 4). Dipisahkan dari command_client.cpp
 * (yang berurusan dengan HTTP/JSON/mbedtls) supaya dapat diuji native
 * (test/test_command_policy) dan agar ada SATU rujukan eksplisit yang harus
 * identik dengan backend Python (backend/app/security.py) — beberapa test di
 * kedua sisi memakai VEKTOR UJI YANG SAMA untuk saling silang-verifikasi.
 *
 * Ancaman yang ditangani modul ini:
 *   - Command dipalsukan/diubah di tengah jalan (MITM, DNS-spoof, backend
 *     yang di-compromise sebagian) -> signature HMAC per-command menolaknya.
 *   - Command lama diputar ulang (replay) setelah kadaluwarsa -> isCommandValidNow().
 *   - Timing attack saat membandingkan signature -> constantTimeHexEqual().
 * ------------------------------------------------------------
 */

#include <cstddef>
#include <cstdint>

namespace commandpolicy {

// Representasi payload RINGKAS & DETERMINISTIK untuk command_type yang
// didukung (lihat models.CommandType backend). HARUS identik dengan
// backend/app/security.py:canonical_command_payload.
//   - "REMOTE_UNLOCK"          -> outBuf = "" (payload diabaikan)
//   - "ALLOW_SLOT"/"REVOKE_SLOT" -> outBuf = "slot=<N>" (perlu hasSlot=true)
// Return false bila command_type tak dikenal, atau ALLOW/REVOKE_SLOT tanpa
// slot, atau outBuf terlalu kecil.
bool canonicalPayload(const char* commandType, bool hasSlot, int32_t slot,
                       char* outBuf, size_t outBufLen);

// String final yang di-HMAC: "<id>.<command_type>.<issued_at>.<expires_at>.<canonicalPayload>"
// HARUS identik dengan backend/app/security.py:command_signature_string.
bool buildSignatureString(const char* commandId, const char* commandType,
                           uint32_t issuedAt, uint32_t expiresAt,
                           const char* canonicalPayload, char* outBuf, size_t outBufLen);

// True hanya jika waktu SEKARANG diketahui (nowUnix!=0, yaitu NTP sudah
// sinkron) DAN belum melewati expiresAt. nowUnix==0 SENGAJA dianggap TIDAK
// valid (fail-closed) — command tidak boleh dieksekusi berdasarkan asumsi
// waktu yang tidak diketahui.
bool isCommandValidNow(uint32_t nowUnix, uint32_t expiresAt);

// Bandingkan dua string hex (case-insensitive) dalam waktu KONSTAN relatif
// terhadap ISI-nya (panjang boleh cepat berbeda, itu bukan rahasia) — untuk
// menghindari timing attack saat memvalidasi signature HMAC.
bool constantTimeHexEqual(const char* a, const char* b);

// CATATAN BATAS FASE: deriveBaseUrl() SENGAJA TIDAK ada di sini walau dulu
// pernah ditaruh di modul ini. Ia dipakai api_client.cpp (Phase 3, untuk
// endpoint heartbeat) MAUPUN command_client.cpp (Phase 4) — menaruhnya di
// src/phase4/ akan membuat api_client.cpp (murni Phase 3) ikut bergantung
// pada folder phase4/, sehingga branch `phase-3` yang menghapus phase4/
// akan gagal compile. Lihat src/url_utils.h (folder src/ level atas,
// dipakai bersama) untuk implementasinya.

}  // namespace commandpolicy
