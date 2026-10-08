#pragma once
/*
 * test/stubs/Preferences.h — NVS palsu TAPI FUNGSIONAL (in-memory),
 * persisten antar instance dalam satu proses test (simulasi reboot).
<<<<<<< HEAD
 *
 * Dua map TERPISAH (32-bit vs 64-bit) meniru penyimpanan bertipe milik
 * Preferences asli ESP32 (getUInt/putUInt vs getULong64/putULong64 adalah
 * API berbeda di hardware sungguhan, bukan sekadar overload).
=======
>>>>>>> origin/main
 */
#include <stdint.h>
#include <string>
#include <map>

std::map<std::string, std::map<std::string, uint32_t>>& __test_fake_nvs();
<<<<<<< HEAD
std::map<std::string, std::map<std::string, uint64_t>>& __test_fake_nvs_u64();
=======
>>>>>>> origin/main
void __test_reset_fake_nvs();

class Preferences {
  public:
    bool begin(const char* ns, bool readOnly = false) {
      (void)readOnly;
      _ns = ns;
      return true;
    }
    void end() {}

    uint32_t getUInt(const char* key, uint32_t defaultValue = 0) {
      auto& nsMap = __test_fake_nvs()[_ns];
      auto it = nsMap.find(key);
      return (it != nsMap.end()) ? it->second : defaultValue;
    }

    size_t putUInt(const char* key, uint32_t value) {
      __test_fake_nvs()[_ns][key] = value;
      return sizeof(uint32_t);
    }

<<<<<<< HEAD
    uint64_t getULong64(const char* key, uint64_t defaultValue = 0) {
      auto& nsMap = __test_fake_nvs_u64()[_ns];
      auto it = nsMap.find(key);
      return (it != nsMap.end()) ? it->second : defaultValue;
    }

    size_t putULong64(const char* key, uint64_t value) {
      __test_fake_nvs_u64()[_ns][key] = value;
      return sizeof(uint64_t);
    }

=======
>>>>>>> origin/main
  private:
    std::string _ns;
};
