/*
 * main.cpp
 * ------------------------------------------------------------
 * Biometric Smart Key & Management System
 * Implementation Phase 1 — Hardware Prototype (Edge Only)
 *
 * Lingkup fase ini (SRS Bab 20, Phase 1):
 *   ESP32 + Sensor Sidik Jari + Servo/Relay + State Machine,
 *   TANPA backend, TANPA jaringan, TANPA database.
 *
 * Tanggung jawab file ini:
 *   1. Inisialisasi Serial (debug/log lokal).
 *   2. Inisialisasi Hardware/Software Watchdog Timer (SRS 5.5)
 *      agar ESP32 otomatis reset bila loop() macet (hang).
 *   3. Menjalankan state machine secara non-blocking di loop().
 * ------------------------------------------------------------
 */

#include <Arduino.h>
#include "config.h"
#include "state_machine.h"

#if defined(ESP32)
  #include <esp_task_wdt.h>
#endif

// Debug command opsional lewat Serial untuk pengujian watchdog:
// ketik 'h' lalu Enter untuk sengaja mem-block loop() dan memicu reset.
static void checkDebugCommands() {
  if (Serial.available() > 0) {
    char c = Serial.peek();
    if (c == 'h') {
      Serial.read();
      Serial.println("[DEBUG] Simulasi HANG dipicu! Loop diblokir. "
                      "Watchdog akan mereset perangkat dalam "
                      + String(WATCHDOG_TIMEOUT_S) + " detik...");
      while (true) {
        // Sengaja diblokir TANPA memanggil esp_task_wdt_reset()
        // untuk membuktikan watchdog benar-benar mereset chip.
        delay(1000);
        Serial.println("[DEBUG] ...masih hang...");
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(300); // beri waktu Serial Monitor connect saat power-up

  Serial.println();
  Serial.println("================================================");
  Serial.println(" Biometric Smart Key & Management System");
  Serial.println(" Phase 1 - Hardware Prototype (Edge Only)");
  Serial.println("================================================");

#if MOCK_FINGERPRINT_MODE
  Serial.println("[BUILD] MOCK_FINGERPRINT_MODE = ON (tanpa sensor fisik)");
#else
  Serial.println("[BUILD] MOCK_FINGERPRINT_MODE = OFF (sensor AS608/R307 aktif)");
#endif

  // ---------------- Watchdog Timer Setup (SRS 5.5) ----------------
  // Catatan: signature esp_task_wdt_init() berbeda antara
  // Arduino-ESP32 core v2.x (ESP-IDF 4.x) dan core v3.x (ESP-IDF 5.x).
  // Blok di bawah menangani keduanya agar tetap compile di kedua versi.
#if defined(ESP32)
  #if ESP_ARDUINO_VERSION_MAJOR >= 3
    esp_task_wdt_config_t wdtConfig = {
      .timeout_ms = WATCHDOG_TIMEOUT_S * 1000,
      .idle_core_mask = 0,
      .trigger_panic = true // panic -> otomatis reboot chip
    };
    esp_task_wdt_init(&wdtConfig);
    esp_task_wdt_add(NULL); // daftarkan task loop() saat ini ke watchdog
  #else
    esp_task_wdt_init(WATCHDOG_TIMEOUT_S, true); // true = panic/reset saat timeout
    esp_task_wdt_add(NULL);
  #endif
  Serial.print("[WATCHDOG] Aktif, timeout = ");
  Serial.print(WATCHDOG_TIMEOUT_S);
  Serial.println(" detik.");
#endif

  // ---------------- State Machine Boot ----------------
  stateMachine.begin();
}

void loop() {
#if defined(ESP32)
  esp_task_wdt_reset(); // "kick" watchdog setiap iterasi loop normal
#endif

  checkDebugCommands();   // 'h' untuk uji watchdog (lihat README)
  stateMachine.update();  // non-blocking state machine tick

  delay(10); // jeda kecil, cukup untuk menjaga responsivitas Serial/GPIO
}
