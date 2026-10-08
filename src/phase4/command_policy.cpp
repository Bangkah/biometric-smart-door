#include "command_policy.h"

#include <cctype>
#include <cstdio>
#include <cstring>

namespace commandpolicy {

bool canonicalPayload(const char* commandType, bool hasSlot, int32_t slot,
                       char* outBuf, size_t outBufLen) {
  if (outBuf == nullptr || outBufLen == 0) return false;

  if (std::strcmp(commandType, "REMOTE_UNLOCK") == 0) {
    outBuf[0] = '\0';
    return true;
  }
  if (std::strcmp(commandType, "ALLOW_SLOT") == 0 || std::strcmp(commandType, "REVOKE_SLOT") == 0) {
    if (!hasSlot) return false;
    int n = std::snprintf(outBuf, outBufLen, "slot=%d", static_cast<int>(slot));
    return n > 0 && static_cast<size_t>(n) < outBufLen;
  }
  return false;  // command_type tak dikenal
}

bool buildSignatureString(const char* commandId, const char* commandType,
                           uint32_t issuedAt, uint32_t expiresAt,
                           const char* canonicalPayload, char* outBuf, size_t outBufLen) {
  if (outBuf == nullptr || outBufLen == 0) return false;
  int n = std::snprintf(outBuf, outBufLen, "%s.%s.%lu.%lu.%s", commandId, commandType,
                         static_cast<unsigned long>(issuedAt), static_cast<unsigned long>(expiresAt),
                         canonicalPayload);
  return n > 0 && static_cast<size_t>(n) < outBufLen;
}

bool isCommandValidNow(uint32_t nowUnix, uint32_t expiresAt) {
  if (nowUnix == 0) return false;  // waktu tidak diketahui -> fail-closed
  return nowUnix < expiresAt;
}

bool constantTimeHexEqual(const char* a, const char* b) {
  size_t la = std::strlen(a), lb = std::strlen(b);
  if (la != lb) return false;  // panjang berbeda boleh keluar cepat (bukan rahasia)

  unsigned char diff = 0;
  for (size_t i = 0; i < la; i++) {
    unsigned char ca = static_cast<unsigned char>(std::tolower(static_cast<unsigned char>(a[i])));
    unsigned char cb = static_cast<unsigned char>(std::tolower(static_cast<unsigned char>(b[i])));
    diff |= static_cast<unsigned char>(ca ^ cb);
  }
  return diff == 0;
}

}  // namespace commandpolicy
