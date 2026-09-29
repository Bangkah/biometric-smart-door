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
