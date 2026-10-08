// test/common_stubs.cpp — implementasi untuk header stub di test/stubs/*.h.
// Diletakkan LANGSUNG di bawah test/ (bukan di subfolder) agar otomatis
// ikut dikompilasi ke SETIAP test suite oleh PlatformIO ("shared code"
// convention: https://docs.platformio.org/en/latest/advanced/unit-testing/structure/shared-code.html).
// test/stubs/stubs.cpp — implementasi untuk seluruh header stub di
// folder ini. Dikompilasi & di-link ke setiap suite native test.
#include "Arduino.h"
#include "LittleFS.h"
#include "Preferences.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <chrono>

// ---- Arduino.h ----
HardwareSerial Serial;
EspClass ESP;

void pinMode(uint8_t, uint8_t) {}
void digitalWrite(uint8_t, uint8_t) {}
int digitalRead(uint8_t) { return 0; }

unsigned long millis() {
  using namespace std::chrono;
  static const auto t0 = steady_clock::now();
  return (unsigned long)duration_cast<milliseconds>(steady_clock::now() - t0).count();
}

void delay(unsigned long) {}
void configTime(long, int, const char*, const char*, const char*) {}

// ---- LittleFS.h ----
LittleFSFS LittleFS;

std::map<std::string, std::vector<uint8_t>>& __test_fake_fs() {
  static std::map<std::string, std::vector<uint8_t>> fs;
  return fs;
}
void __test_reset_fake_fs() { __test_fake_fs().clear(); }

// ---- Preferences.h ----
std::map<std::string, std::map<std::string, uint32_t>>& __test_fake_nvs() {
  static std::map<std::string, std::map<std::string, uint32_t>> nvs;
  return nvs;
}
<<<<<<< HEAD
std::map<std::string, std::map<std::string, uint64_t>>& __test_fake_nvs_u64() {
  static std::map<std::string, std::map<std::string, uint64_t>> nvs64;
  return nvs64;
}
void __test_reset_fake_nvs() {
  __test_fake_nvs().clear();
  __test_fake_nvs_u64().clear();
}
=======
void __test_reset_fake_nvs() { __test_fake_nvs().clear(); }
>>>>>>> origin/main

// ---- freertos ----
SemaphoreHandle_t xSemaphoreCreateMutex() { return reinterpret_cast<SemaphoreHandle_t>(1); }
BaseType_t xSemaphoreTake(SemaphoreHandle_t, TickType_t) { return pdTRUE; }
BaseType_t xSemaphoreGive(SemaphoreHandle_t) { return pdTRUE; }
void vTaskDelay(TickType_t) {}
