/*
 * main.cpp
<<<<<<< HEAD
 * ------------------------------------------------------------
 * Biometric Smart Key & Management System
 * Implementation Phase 1 (Hardware Prototype) +
 * Implementation Phase 2 (Local Event System & Memory Buffer) +
 * Implementation Phase 3 (Network, NTP Time Sync & Backend Event Upload) +
 * Implementation Phase 4 (Remote Command: ALLOW/REVOKE_SLOT, REMOTE_UNLOCK)
 *
 * Lingkup (SRS Bab 20, Phase 1-4):
 *   ESP32 + Sensor Sidik Jari + Servo/Relay + State Machine +
 *   Local Event Logging (LittleFS ring buffer) + Wi-Fi/NTP + upload
 *   event ke backend via HTTPS + polling command jarak jauh (Phase 4,
 *   aktif/nonaktif lewat ENABLE_PHASE4_FEATURES di config.h -- lihat
 *   wiki/Architecture.md bagian "Batas Fase").
=======
 * Biometric Smart Key & Management System
 * Implementation Phase 1 (Hardware Prototype) +
 * Implementation Phase 2 (Local Event System & Memory Buffer) +
 * Implementation Phase 3 (Network, NTP Time Sync & Backend Event Upload)
 *
 * Lingkup (SRS Bab 20, Phase 1-3):
 *   ESP32 + Sensor Sidik Jari + Servo/Relay + State Machine +
 *   Local Event Logging (LittleFS ring buffer) + Wi-Fi/NTP + upload
 *   event ke backend via HTTPS.
>>>>>>> origin/main
 *
 * Arsitektur eksekusi (menjaga Edge Autonomy, SRS 2.2 / NFR-002):
 *   - loop()  (core 1): watchdog + debugger Serial + state machine.
 *                       TIDAK PERNAH menyentuh jaringan.
 *   - net task (core 0): Wi-Fi manager + NTP + upload backend. HTTP/TLS
 *                       boleh lambat/menggantung; loop() tidak terpengaruh.
 *                       Task ini SENGAJA tidak didaftarkan ke task watchdog:
 *                       jaringan macet tidak boleh me-reset kunci pintu.
 *                       Semua panggilan jaringan dibatasi timeout.
 *
 * Tanggung jawab file ini:
 *   1. Inisialisasi Serial (debug/log lokal).
 *   2. Inisialisasi Hardware/Software Watchdog Timer (SRS 5.5)
 *      agar ESP32 otomatis reset bila loop() macet (hang).
 *   3. Inisialisasi Event Logger (SRS 5.6) SEBELUM state machine
 *      dimulai, agar event SYSTEM_BOOT dapat tercatat sejak awal.
 *   4. Menjalankan state machine secara non-blocking di loop().
 *   5. Menyediakan perintah debugger Serial: 'h' (uji watchdog),
 *      'l' (lihat log), 'c' (hapus log), 'w' (status Wi-Fi/NTP/sync),
 *      's' (sync log ke backend sekarang).
 *   6. [Phase 3] Menjalankan network task terpisah.
<<<<<<< HEAD
 * ------------------------------------------------------------
=======
>>>>>>> origin/main
 */

#include <Arduino.h>
#include "config.h"
#include "state_machine.h"
#include "event_logger.h"
#include "network_manager.h"
#include "time_sync.h"
#include "api_client.h"

<<<<<<< HEAD
#if ENABLE_PHASE4_FEATURES
#include "phase4/access_policy.h"
#include "phase4/command_client.h"
#endif

=======
>>>>>>> origin/main
#if defined(ESP32)
  #include <esp_task_wdt.h>
#endif

// [Phase 3] Penyedia waktu UNIX untuk event_logger (dependency injection).
static uint32_t unixTimeProvider() {
  return static_cast<uint32_t>(timeSync.nowUnix());  // 0 bila NTP belum sinkron
}

static void printNetworkStatus() {
#if NETWORK_ENABLED
  networkManager.printStatus();
  Serial.println("---------- NTP ----------");
  Serial.printf("Waktu     : %s\n", timeSync.getFormattedTime().c_str());
  apiClient.printStatus();
<<<<<<< HEAD
#if ENABLE_PHASE4_FEATURES
  commandClient.printStatus();
#endif
#else
  Serial.println("[DEBUG] NETWORK_ENABLED=0: jaringan dinonaktifkan pada build ini.");
#endif
#if ENABLE_PHASE4_FEATURES
  Serial.printf("[ACCESS_POLICY] mask=0x%016llX (bit=1 berarti slot diizinkan)\n",
                (unsigned long long)accessPolicy.snapshotMask());
#endif
=======
#else
  Serial.println("[DEBUG] NETWORK_ENABLED=0: jaringan dinonaktifkan pada build ini.");
#endif
>>>>>>> origin/main
  Serial.printf("Free heap : %u byte (min sejak boot: %u)\n",
                (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMinFreeHeap());
}

#if NETWORK_ENABLED
// [Phase 3] Seluruh I/O jaringan hidup di sini, terpisah dari loop().
// Setiap modul hanya melakukan pekerjaan singkat per tick, kecuali
// apiClient.update() yang boleh memblokir task INI (bukan loop()) selama
// satu request HTTP — dibatasi HTTP_CONNECT_TIMEOUT_MS + HTTP_REQUEST_TIMEOUT_MS.
static void networkTask(void* /*param*/) {
  for (;;) {
    networkManager.update();
    timeSync.update();
    apiClient.update();
<<<<<<< HEAD
#if ENABLE_PHASE4_FEATURES
    commandClient.update();  // polling command jarak jauh (SRS 10.5, 10.6)
#endif
=======
>>>>>>> origin/main
    vTaskDelay(pdMS_TO_TICKS(NET_TASK_TICK_MS));
  }
}
#endif

// Perintah debug interaktif via Serial Monitor:
//   'h' -> simulasi HANG untuk menguji watchdog (Phase 1)
//   'l' -> tampilkan seluruh event log tersimpan (Phase 2)
//   'c' -> hapus/reset seluruh event log (Phase 2)
//   'w' -> status Wi-Fi, IP, NTP, dan antrean sync ke backend (Phase 3)
//   's' -> minta sinkronisasi log ke backend SEKARANG (Phase 3)
// (Perintah 't' / 'm' / 'x' untuk simulasi sidik jari ditangani
// terpisah di fingerprint.cpp saat MOCK_FINGERPRINT_MODE aktif.)
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
    } else if (c == 'l') {
      Serial.read();
      eventLogger.dumpToSerial();
    } else if (c == 'c') {
      Serial.read();
      Serial.println("[DEBUG] Menghapus seluruh event log...");
      eventLogger.clearAll();
    } else if (c == 'w') {
      Serial.read();
      printNetworkStatus();
    } else if (c == 's') {
      Serial.read();
#if NETWORK_ENABLED
      Serial.println("[DEBUG] Sinkronisasi manual diminta (dijalankan di network task)...");
      apiClient.requestSyncNow();
#else
      Serial.println("[DEBUG] NETWORK_ENABLED=0: jaringan dinonaktifkan pada build ini.");
#endif
    } else if (c == 't' || c == 'm' || c == 'x') {
      // dikonsumsi oleh fingerprintModule.verify()/isFingerPresent()
      // saat MOCK_FINGERPRINT_MODE aktif -> jangan dibuang di sini.
      return;
    } else {
      Serial.read(); // buang karakter tak dikenal (mis. newline sisa)
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(300); // beri waktu Serial Monitor connect saat power-up

  Serial.println();
  Serial.println("================================================");
  Serial.println(" Biometric Smart Key & Management System");
<<<<<<< HEAD
  Serial.println(" Phase 1 (Hardware) + 2 (Event Log) + 3 (Network/NTP/Sync) + 4 (Remote Command)");
=======
  Serial.println(" Phase 1 (Hardware) + 2 (Event Log) + 3 (Network/NTP/Sync)");
>>>>>>> origin/main
  Serial.println(" Firmware " FIRMWARE_VERSION);
  Serial.println("================================================");

#if MOCK_FINGERPRINT_MODE
  Serial.println("[BUILD] MOCK_FINGERPRINT_MODE = ON (tanpa sensor fisik)");
#else
  Serial.println("[BUILD] MOCK_FINGERPRINT_MODE = OFF (sensor AS608/R307 aktif)");
#endif

<<<<<<< HEAD
  // ---------------- Watchdog Timer Setup (SRS 5.5) ----------------
=======
  // Watchdog Timer Setup (SRS 5.5) 
>>>>>>> origin/main
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

<<<<<<< HEAD
  // ---------------- Event Logger Init (SRS 5.6, Phase 2) ----------------
=======
  // Event Logger Init (SRS 5.6, Phase 2) 
>>>>>>> origin/main
  // WAJIB sebelum stateMachine.begin(), karena runBoot() langsung
  // mencatat event SYSTEM_BOOT.
  eventLogger.setTimeProvider(unixTimeProvider);  // Phase 3: timestamp UNIX riil bila NTP sinkron
  eventLogger.begin();

<<<<<<< HEAD
#if ENABLE_PHASE4_FEATURES
  // Tidak butuh jaringan (murni baca NVS) -> aman diinisialisasi sebelum
  // Wi-Fi, dan tetap berfungsi penuh walau device offline selamanya.
  accessPolicy.begin();
#endif

  // ---------------- Network (Phase 3) ----------------
=======
  // Network (Phase 3) 
>>>>>>> origin/main
  // Dimulai SETELAH event logger siap, dan SEBELUM state machine agar upload
  // dapat berjalan paralel sejak awal. Jika WIFI_SSID kosong, semuanya idle
  // dan perangkat berperilaku persis seperti Phase 1/2.
#if NETWORK_ENABLED
  networkManager.begin();   // non-blocking: hanya memulai percobaan koneksi
  timeSync.begin();
  apiClient.begin();
<<<<<<< HEAD
#if ENABLE_PHASE4_FEATURES
  commandClient.begin();
#endif
=======
>>>>>>> origin/main
  BaseType_t taskOk = xTaskCreatePinnedToCore(networkTask, "net_task", NET_TASK_STACK_BYTES,
                                              nullptr, NET_TASK_PRIORITY, nullptr, NET_TASK_CORE);
  if (taskOk != pdPASS) {
    Serial.println("[NETWORK] GAGAL membuat network task -> berjalan offline-only.");
  }
#else
  Serial.println("[NETWORK] NETWORK_ENABLED=0 -> tanpa jaringan.");
#endif

  Serial.println("[DEBUG] Perintah: 'l'=lihat log, 'c'=hapus log, 'w'=status jaringan, "
                 "'s'=sync ke backend, 'h'=uji watchdog");
<<<<<<< HEAD
#if ENABLE_PHASE4_FEATURES
  Serial.println("[DEBUG] Phase 4 aktif: command jarak jauh (REMOTE_UNLOCK/ALLOW_SLOT/REVOKE_SLOT) "
                 "di-poll otomatis tiap COMMAND_POLL_INTERVAL_MS -- tidak perlu perintah Serial tambahan, "
                 "lihat status via 'w'.");
#endif

  // ---------------- State Machine Boot ----------------
=======

  // State Machine Boot 
>>>>>>> origin/main
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
