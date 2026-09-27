#include "fingerprint.h"
#include "config.h"

FingerprintModule fingerprintModule;

// ================================================================
// MODE 1: MOCK (tanpa sensor fisik) — Serial Monitor + Push Button
// ================================================================
#if MOCK_FINGERPRINT_MODE

void FingerprintModule::begin() {
  pinMode(MOCK_TOUCH_BUTTON_PIN, INPUT_PULLUP);
  _sensorReady = true;
  Serial.println("[FINGERPRINT] MOCK MODE aktif.");
  Serial.println("[FINGERPRINT] Kontrol simulasi:");
  Serial.println("               - Serial: ketik 't' lalu Enter untuk simulasi sentuhan jari");
  Serial.println("               - Serial: ketik 'm' lalu Enter untuk simulasi MATCH");
  Serial.println("               - Serial: ketik 'x' lalu Enter untuk simulasi NO MATCH");
  Serial.println("               - Tombol fisik di MOCK_TOUCH_BUTTON_PIN: tekan singkat = sentuhan jari");
}

bool FingerprintModule::healthCheck() {
  // Dalam mode mock, sensor "selalu sehat" agar alur HARDWARE_CHECK
  // dapat diuji end-to-end tanpa perangkat keras sensor.
  return true;
}

bool FingerprintModule::isFingerPresent() {
  // Tombol fisik ditekan (active LOW)?
  if (digitalRead(MOCK_TOUCH_BUTTON_PIN) == LOW) {
    delay(30); // debounce sederhana
    if (digitalRead(MOCK_TOUCH_BUTTON_PIN) == LOW) {
      return true;
    }
  }

  // Perintah dari Serial Monitor?
  if (Serial.available() > 0) {
    char c = Serial.peek();
    if (c == 't' || c == 'm' || c == 'x') {
      return true; // biarkan verify() yang mengonsumsi karakter & menentukan hasil
    } else {
      Serial.read(); // buang karakter tak dikenal
    }
  }

  return false;
}

FingerprintResult FingerprintModule::verify() {
  unsigned long start = millis();

  // Jika dipicu oleh tombol fisik (bukan Serial), hasilnya selalu MATCH
  // dengan slot dummy #1, supaya bisa uji alur UNLOCKED langsung.
  if (digitalRead(MOCK_TOUCH_BUTTON_PIN) == LOW && !Serial.available()) {
    _lastMatchedSlot = 1;
    Serial.println("[FINGERPRINT] (mock/button) -> MATCH (slot #1)");
    return FingerprintResult::MATCH;
  }

  // Tunggu 1 karakter perintah dari Serial, dengan timeout supaya
  // firmware tidak hang selamanya (menjaga fail-safe & watchdog).
  while (millis() - start < FP_VERIFY_TIMEOUT_MS) {
    if (Serial.available() > 0) {
      char c = Serial.read();
      if (c == 'm') {
        _lastMatchedSlot = 1;
        Serial.println("[FINGERPRINT] (mock/serial) -> MATCH (slot #1)");
        return FingerprintResult::MATCH;
      } else if (c == 'x') {
        _lastMatchedSlot = -1;
        Serial.println("[FINGERPRINT] (mock/serial) -> NO MATCH");
        return FingerprintResult::NO_MATCH;
      } else if (c == 't') {
        // 't' hanya menandai "sentuhan", tunggu perintah berikutnya
        continue;
      }
      // karakter lain diabaikan
    }
  }

  Serial.println("[FINGERPRINT] (mock) -> timeout menunggu input, dianggap NO_MATCH");
  return FingerprintResult::NO_MATCH;
}

// ================================================================
// MODE 2: SENSOR ASLI (AS608 / R307) via Adafruit Fingerprint Library
// ================================================================
#else

#include <Adafruit_Fingerprint.h>
#include <HardwareSerial.h>

static HardwareSerial fpSerial(2); // UART2
static Adafruit_Fingerprint finger(&fpSerial);

void FingerprintModule::begin() {
  fpSerial.begin(FP_UART_BAUD, SERIAL_8N1, FP_RX_PIN, FP_TX_PIN);
  finger.begin(FP_UART_BAUD);
  _sensorReady = finger.verifyPassword();

  if (_sensorReady) {
    Serial.println("[FINGERPRINT] Sensor AS608/R307 terdeteksi & siap.");
  } else {
    Serial.println("[FINGERPRINT] GAGAL terhubung ke sensor!");
  }
}

bool FingerprintModule::healthCheck() {
  _sensorReady = finger.verifyPassword();
  return _sensorReady;
}

bool FingerprintModule::isFingerPresent() {
  if (!_sensorReady) return false;
  int p = finger.getImage();
  return (p == FINGERPRINT_OK);
}

FingerprintResult FingerprintModule::verify() {
  if (!_sensorReady) return FingerprintResult::SENSOR_ERROR;

  unsigned long start = millis();

  // Ambil citra (hanya untuk memicu proses; citra tidak pernah
  // meninggalkan modul sensor / dikirim ke luar — SRS 12.6).
  int p = finger.getImage();
  if (p == FINGERPRINT_NOFINGER) {
    return FingerprintResult::NO_FINGER;
  } else if (p != FINGERPRINT_OK) {
    Serial.println("[FINGERPRINT] Error saat getImage()");
    return FingerprintResult::SENSOR_ERROR;
  }

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) {
    Serial.println("[FINGERPRINT] Error saat image2Tz()");
    return FingerprintResult::SENSOR_ERROR;
  }

  p = finger.fingerFastSearch();

  if (millis() - start > FP_VERIFY_TIMEOUT_MS) {
    Serial.println("[FINGERPRINT] Timeout verifikasi (> FP_VERIFY_TIMEOUT_MS)");
    return FingerprintResult::NO_MATCH;
  }

  if (p == FINGERPRINT_OK) {
    _lastMatchedSlot = finger.fingerID;
    Serial.print("[FINGERPRINT] MATCH slot #");
    Serial.println(_lastMatchedSlot);
    return FingerprintResult::MATCH;
  } else if (p == FINGERPRINT_NOTFOUND) {
    _lastMatchedSlot = -1;
    Serial.println("[FINGERPRINT] NO MATCH");
    return FingerprintResult::NO_MATCH;
  }

  Serial.println("[FINGERPRINT] Error saat fingerFastSearch()");
  return FingerprintResult::SENSOR_ERROR;
}

#endif // MOCK_FINGERPRINT_MODE
