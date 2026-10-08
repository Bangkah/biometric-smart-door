#pragma once
/*
 * url_utils.h
 * ------------------------------------------------------------
 * Fungsi URL murni (tanpa Arduino/HTTP), header-only (pola sama dengan
 * backoff.h/time_estimate.h) — milik Phase 3, sengaja ditaruh di src/
 * (BUKAN src/phase4/) karena api_client.cpp (Phase 3, heartbeat) MEMBUTUHKANNYA.
 *
 * command_client.cpp (Phase 4) juga memakainya, dan itu TIDAK masalah —
 * boleh bergantung "ke bawah" pada Phase 3. Yang tidak boleh adalah
 * sebaliknya: kode Phase 3 bergantung pada sesuatu di src/phase4/, karena
 * itu akan membuat branch `phase-3` gagal compile begitu folder phase4/
 * dihapus (lihat wiki/Architecture.md bagian "Batas Fase").
 * ------------------------------------------------------------
 */

#include <cstddef>
#include <cstring>

namespace urlutils {

// Menurunkan base URL backend ("https://host") dari BACKEND_EVENTS_ENDPOINT
// ("https://host/api/v1/events") dengan memotong akhiran path yang
// diharapkan. Dipakai api_client.cpp untuk endpoint heartbeat, dan
// command_client.cpp untuk endpoint commands/poll & commands/{id}/ack —
// supaya secrets.h Phase 3 yang sudah ada tetap kompatibel tanpa field URL
// tambahan. Return false bila eventsUrl tidak diakhiri "/api/v1/events"
// atau outBuf terlalu kecil.
inline bool deriveBaseUrl(const char* eventsUrl, char* outBuf, size_t outBufLen) {
  static const char* SUFFIX = "/api/v1/events";
  size_t urlLen = std::strlen(eventsUrl);
  size_t sufLen = std::strlen(SUFFIX);
  if (urlLen <= sufLen) return false;

  size_t baseLen = urlLen - sufLen;
  if (std::strcmp(eventsUrl + baseLen, SUFFIX) != 0) return false;
  if (baseLen + 1 > outBufLen) return false;

  std::memcpy(outBuf, eventsUrl, baseLen);
  outBuf[baseLen] = '\0';
  return true;
}

}  // namespace urlutils
