#pragma once
/*
 * test/stubs/Arduino.h — pengganti Arduino core untuk env:native.
 * Cukup fungsional untuk event_logger.cpp (dan modul lain yang
 * hanya memakai subset dasar ini): String, Serial/Print, millis().
 * BUKAN untuk produksi — hanya dipakai oleh `pio test -e native`.
 */
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <string>

#define HIGH 1
#define LOW  0
#define OUTPUT 1
#define INPUT_PULLUP 2

void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, uint8_t value);
int  digitalRead(uint8_t pin);

unsigned long millis();
void delay(unsigned long ms);
void configTime(long gmtOffsetSec, int dstOffsetSec, const char* s1,
                 const char* s2 = nullptr, const char* s3 = nullptr);

class String {
  public:
    String() {}
    String(const char* s) : _s(s ? s : "") {}
    String(const std::string& s) : _s(s) {}
    String(int v) : _s(std::to_string(v)) {}
    String(unsigned v) : _s(std::to_string(v)) {}
    String(long v) : _s(std::to_string(v)) {}
    String(unsigned long v) : _s(std::to_string(v)) {}
    const char* c_str() const { return _s.c_str(); }
    unsigned length() const { return (unsigned)_s.size(); }
  private:
    std::string _s;
};

class Print {
  public:
    size_t print(const char* s)   { return fputs(s, stdout) >= 0 ? strlen(s) : 0; }
    size_t print(const String& s) { return print(s.c_str()); }
    size_t println(const char* s) { size_t n = print(s); putchar('\n'); return n + 1; }
    size_t println(const String& s) { return println(s.c_str()); }
    size_t println()              { putchar('\n'); return 1; }
    size_t printf(const char* fmt, ...) __attribute__((format(printf, 2, 3))) {
      va_list args;
      va_start(args, fmt);
      int n = vprintf(fmt, args);
      va_end(args);
      return n > 0 ? (size_t)n : 0;
    }
};

class HardwareSerial : public Print {
  public:
    void begin(unsigned long) {}
    int available() { return 0; }
    int peek() { return -1; }
    int read() { return -1; }
};
extern HardwareSerial Serial;

class EspClass {
  public:
    uint32_t getFreeHeap() { return 200000; }
    uint32_t getMinFreeHeap() { return 180000; }
};
extern EspClass ESP;
