#pragma once
/*
 * config.h
 * ------------------------------------------------------------
 * Konfigurasi terpusat: pin mapping, timing, dan build mode.
 * Sesuai SRS Bab 4 (Hardware) dan Bab 5 (Firmware).
 * ------------------------------------------------------------
 */

#include <Arduino.h>

// ============================================================
// BUILD MODE
// ============================================================
// Aktifkan baris di bawah ini jika sensor sidik jari FISIK belum
// terpasang. Firmware akan mensimulasikan pembacaan sidik jari
// melalui Serial Monitor dan/atau push button (lihat fingerprint.cpp
// dan README.md bagian "Bench Testing Tanpa Sensor Fisik").
//
// Nonaktifkan (comment out) baris ini saat sensor AS608/R307 asli
// sudah terpasang dan siap diuji end-to-end.
#define MOCK_FINGERPRINT_MODE 1

// Aktifkan jika aktuator yang dipakai adalah Servo (prototipe meja).
// Jika dinonaktifkan, firmware akan mengasumsikan Relay + Solenoid
// Door Lock 12V (SRS 4.3).
// #define ACTUATOR_TYPE_SERVO 1

// ============================================================
// PIN MAPPING
// ============================================================

// --- Fingerprint sensor (AS608 / R307) via UART2 ---
// Tidak digunakan saat MOCK_FINGERPRINT_MODE aktif, tapi tetap
// didefinisikan agar transisi ke hardware asli tinggal comment 1 baris.
#define FP_RX_PIN            16   // ESP32 RX2 <-- Sensor TX
#define FP_TX_PIN            17   // ESP32 TX2 --> Sensor RX
#define FP_UART_BAUD         57600

// --- Actuator ---
#define RELAY_PIN            26   // Relay module -> Solenoid Door Lock 12V
#define SERVO_PIN            27   // Hanya dipakai jika ACTUATOR_TYPE_SERVO
#define SERVO_LOCKED_ANGLE   0
#define SERVO_UNLOCKED_ANGLE 90

// Relay logic level. Banyak modul relay murah bersifat active-LOW.
#define RELAY_ACTIVE_LOW     1

// --- Indicators ---
#define LED_RED_PIN           25   // DENIED / ERROR_SAFE
#define LED_GREEN_PIN         33   // UNLOCKED / MATCH
#define LED_BLUE_PIN          32   // IDLE / VERIFYING (siaga/proses)

// --- Mock bench-test input ---
// Push button untuk mensimulasikan "sentuhan sensor" saat sensor fisik
// belum terpasang. Gunakan INPUT_PULLUP -> tombol menghubungkan ke GND.
#define MOCK_TOUCH_BUTTON_PIN 4

// ============================================================
// TIMING / BEHAVIOR (sesuai SRS Bab 5, 6, FR-001, FR-005)
// ============================================================
#define UNLOCK_DURATION_MS       5000UL  // SRS 5.3: auto-lock setelah 5 detik
#define FP_VERIFY_TIMEOUT_MS     2000UL  // FR-001: verifikasi < 2 detik
#define ERROR_RECOVERY_DELAY_MS  3000UL  // Jeda sebelum retry HARDWARE_CHECK dari ERROR_SAFE
#define DENIED_DISPLAY_MS        1500UL  // Lama indikator merah sebelum kembali ke IDLE

// ============================================================
// WATCHDOG (SRS 5.5)
// ============================================================
#define WATCHDOG_TIMEOUT_S       8    // Reset otomatis jika loop() macet > 8 detik

// ============================================================
// LOCAL EVENT LOGGING / PERSISTENT BUFFER (SRS 5.6, Phase 2)
// ============================================================
// Ring buffer disimpan di LittleFS sebagai file berukuran tetap
// (EVENT_LOG_MAX_ENTRIES * sizeof(EventRecord)) agar posisi setiap
// slot dapat dihitung langsung (tidak perlu parsing), dan tahan
// terhadap pemadaman listrik karena setiap logEvent() langsung
// ditulis+di-flush ke flash (bukan disangga di RAM).
#define EVENT_LOG_FILE            "/events.dat"
#define EVENT_LOG_MAX_ENTRIES     50    // SRS: 50-100 log terakhir; ubah ke 100 bila perlu
#define EVENT_LOG_NVS_NAMESPACE   "evtlog"
#define EVENT_LOG_LOCK_TIMEOUT_MS 1000  // batas tunggu mutex ring buffer (loop <-> net task)

// ============================================================
// PHASE 3 — NETWORK, NTP & BACKEND EVENT UPLOAD
// ============================================================
#define FIRMWARE_VERSION          "0.3.0-phase3"

// ---- Kredensial & endpoint (satu-satunya tempat secrets di-include) ----
// include/secrets.h TIDAK di-commit (.gitignore). Template: secrets.h.example.
// Jika secrets.h tidak ada (mis. GitHub Actions), nilai kosong dipakai:
// firmware tetap compile dan berjalan OFFLINE-ONLY seperti Phase 1/2
// (WIFI_SSID kosong => modul jaringan tidak pernah mencoba konek).
#if __has_include("secrets.h")
  #include "secrets.h"
#endif
#ifndef WIFI_SSID
#define WIFI_SSID                 ""
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD             ""
#endif
#ifndef DEVICE_API_TOKEN
#define DEVICE_API_TOKEN          ""      // Bearer token unik per perangkat (SRS 12.3)
#endif
#ifndef DEVICE_ID
#define DEVICE_ID                 "DOOR-01"
#endif
#ifndef BACKEND_EVENTS_ENDPOINT
#define BACKEND_EVENTS_ENDPOINT   "https://your-backend.example.com/api/v1/events"
#endif
#ifndef BACKEND_ROOT_CA
#define BACKEND_ROOT_CA           ""      // PEM root CA backend untuk verifikasi TLS
#endif

// Saklar global jaringan (0 = Phase 1/2 murni, tanpa task jaringan sama sekali).
#define NETWORK_ENABLED           1

// ---- Kebijakan transport (SRS NFR-001: seluruh komunikasi wajib TLS) ----
// Default AMAN. Dilonggarkan HANYA lewat build env dev/bench (platformio.ini).
#ifndef BACKEND_ALLOW_PLAIN_HTTP
#define BACKEND_ALLOW_PLAIN_HTTP  0       // 1 = izinkan http:// (env esp32dev-bench)
#endif
#ifndef BACKEND_TLS_INSECURE
#define BACKEND_TLS_INSECURE      0       // 1 = https TANPA verifikasi sertifikat (dev saja)
#endif

// ---- Wi-Fi Connection Manager ----
#define WIFI_CONNECT_TIMEOUT_MS   15000UL // batas satu percobaan koneksi
#define WIFI_BACKOFF_BASE_MS       2000UL // delay awal sebelum retry
#define WIFI_BACKOFF_MAX_MS       60000UL // batas atas exponential backoff

// ---- Network task (FreeRTOS) ----
// SEMUA I/O jaringan (Wi-Fi, NTP, HTTP/TLS) berjalan di task ini, BUKAN di
// loop(). Jadi state machine, verifikasi sidik jari, dan watchdog tidak
// pernah ikut terblokir oleh jaringan lambat (SRS 2.2 Edge Autonomy, NFR-002).
#define NET_TASK_STACK_BYTES      12288
#define NET_TASK_PRIORITY         1
#define NET_TASK_CORE             0       // loop() Arduino berjalan di core 1
#define NET_TASK_TICK_MS          50

// ---- NTP ----
#define NTP_SERVER_1              "pool.ntp.org"
#define NTP_SERVER_2              "time.google.com"
#define NTP_SERVER_3              "time.cloudflare.com"
#define NTP_GMT_OFFSET_SEC        (7 * 3600)  // WIB (UTC+7) — hanya memengaruhi TAMPILAN lokal;
                                              // epoch yang disimpan & dikirim ke backend selalu UTC
#define NTP_DAYLIGHT_OFFSET_SEC   0
#define NTP_RETRY_INTERVAL_MS     30000UL
#define MIN_VALID_UNIX_TIME       1735689600UL // 2025-01-01T00:00:00Z; di bawah ini = belum sinkron

// ---- Backend upload (SRS 10.2, FR-009) ----
#define HTTP_CONNECT_TIMEOUT_MS   5000UL
#define HTTP_REQUEST_TIMEOUT_MS   8000UL
#define SYNC_INTERVAL_MS          30000UL // auto-sync berkala saat ada event baru
#define SYNC_BATCH_MAX_ENTRIES    10      // maksimum event per POST
#define SYNC_RETRY_BASE_MS        5000UL  // backoff eksponensial saat upload gagal
#define SYNC_RETRY_MAX_MS         300000UL
// Tunda upload sampai NTP sinkron: event sesi ini bisa diberi timestamp
// UNIX (estimasi) dan verifikasi sertifikat TLS memakai waktu yang benar.
#define SYNC_REQUIRE_TIME         1
#define API_CLIENT_NVS_NAMESPACE  "apiclient"
<<<<<<< HEAD

// ---- Device Heartbeat (SRS 8.2/10.3, FR-015) ----
// SENGAJA konstanta terpisah dari SYNC_INTERVAL_MS: heartbeat adalah
// telemetry murni, tidak boleh terikat pada siklus/timing sync event
// (lihat api_client.h dan wiki/Architecture.md).
#define HEARTBEAT_INTERVAL_MS     60000UL  // 1 menit

// ============================================================
// PHASE BOUNDARY — Phase 4: Remote User Sync & Remote Unlock
// ============================================================
// Modul di src/phase4/ (access_policy, command_policy, command_client) DAN
// blok kode yang dipagari makro ini di file lain (fingerprint.cpp,
// state_machine.cpp, main.cpp) adalah lingkup Phase 4, terpisah secara
// fisik dari Phase 1-3 agar mudah di-branch (lihat wiki/Contributing.md
// untuk strategi Git phase-3 vs phase-4). Default AKTIF (proyek ini sedang
// mengerjakan Phase 4). Set ke 0 untuk build "Phase 3 murni": fingerprint
// mock/real tidak lagi mengecek access_policy (semua slot yang cocok
// sensor otomatis MATCH, seperti Phase 1-3 asli), dan command_client tidak
// diinisialisasi/dijalankan sama sekali.
#ifndef ENABLE_PHASE4_FEATURES
#define ENABLE_PHASE4_FEATURES 1
#endif

// ---- Access Policy (SRS 7.4 Revocation, FR-013, Phase 4) ----
#define ACCESS_POLICY_NVS_NAMESPACE "accesspol"

// ---- Command Client (SRS 10.5/10.6, 12.5, Phase 4) ----
// Endpoint /commands/poll & /commands/{id}/ack diturunkan dari
// BACKEND_EVENTS_ENDPOINT (commandpolicy::deriveBaseUrl), sama seperti heartbeat.
#define COMMAND_POLL_INTERVAL_MS       5000UL   // cukup responsif untuk demo remote-unlock
#define COMMAND_HTTP_CONNECT_TIMEOUT_MS 5000UL
#define COMMAND_HTTP_REQUEST_TIMEOUT_MS 8000UL
#define COMMAND_RETRY_BASE_MS          5000UL
#define COMMAND_RETRY_MAX_MS           60000UL
#define COMMAND_CLIENT_NVS_NAMESPACE   "cmdclient"
#define COMMAND_ID_MAX_LEN             40   // UUID (36) + margin
#define COMMAND_TYPE_MAX_LEN           20
#define COMMAND_SIGNATURE_HEX_LEN      64   // SHA-256 -> 32 byte -> 64 hex char
#ifndef DEVICE_HMAC_SECRET
#define DEVICE_HMAC_SECRET ""   // dari secrets.h; kosong = command client nonaktif (fail-closed)
#endif
=======
>>>>>>> origin/main
