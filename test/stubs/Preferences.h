#pragma once
/*
 * test/stubs/Preferences.h — NVS palsu TAPI FUNGSIONAL (in-memory),
 * persisten antar instance dalam satu proses test (simulasi reboot).
 */
#include <stdint.h>
#include <string>
#include <map>

std::map<std::string, std::map<std::string, uint32_t>>& __test_fake_nvs();
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

  private:
    std::string _ns;
};
