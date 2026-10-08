#include "state_machine.h"
#include "config.h"
#include "fingerprint.h"
#include "actuator.h"
#include "event_logger.h"

<<<<<<< HEAD
#if ENABLE_PHASE4_FEATURES
#include "phase4/command_client.h"
#endif

=======
>>>>>>> origin/main
StateMachine stateMachine;

const char* stateToString(SystemState state) {
  switch (state) {
    case SystemState::BOOT:            return "BOOT";
    case SystemState::INITIALIZE:      return "INITIALIZE";
    case SystemState::HARDWARE_CHECK:  return "HARDWARE_CHECK";
    case SystemState::LOCKED:          return "LOCKED";
    case SystemState::IDLE:            return "IDLE";
    case SystemState::VERIFYING:       return "VERIFYING";
    case SystemState::UNLOCKED:        return "UNLOCKED";
    case SystemState::DENIED:          return "DENIED";
    case SystemState::ERROR_SAFE:      return "ERROR_SAFE";
    default:                           return "UNKNOWN";
  }
}

void StateMachine::begin() {
  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(LED_GREEN_PIN, OUTPUT);
  pinMode(LED_BLUE_PIN, OUTPUT);
  enterState(SystemState::BOOT);
}

void StateMachine::enterState(SystemState newState) {
  Serial.print("[STATE] ");
  Serial.print(stateToString(_state));
  Serial.print(" -> ");
  Serial.println(stateToString(newState));

  _state = newState;
  _stateEnteredAt = millis();
}

void StateMachine::setIndicator(bool red, bool green, bool blue) {
  digitalWrite(LED_RED_PIN, red ? HIGH : LOW);
  digitalWrite(LED_GREEN_PIN, green ? HIGH : LOW);
  digitalWrite(LED_BLUE_PIN, blue ? HIGH : LOW);
}

// Dipanggil setiap loop() dari main.cpp. Non-blocking: setiap "run*"
// hanya melakukan sedikit pekerjaan lalu kembali, transisi state
// terjadi berdasarkan kondisi/waktu, bukan delay() panjang.
void StateMachine::update() {
  switch (_state) {
    case SystemState::BOOT:           runBoot(); break;
    case SystemState::INITIALIZE:     runInitialize(); break;
    case SystemState::HARDWARE_CHECK: runHardwareCheck(); break;
    case SystemState::LOCKED:         runLocked(); break;
    case SystemState::IDLE:           runIdle(); break;
    case SystemState::VERIFYING:      runVerifying(); break;
    case SystemState::UNLOCKED:       runUnlocked(); break;
    case SystemState::DENIED:         runDenied(); break;
    case SystemState::ERROR_SAFE:     runErrorSafe(); break;
  }
}

<<<<<<< HEAD
// ----------------------------------------------------------------
// BOOT: titik masuk tunggal, baik dari cold start maupun dari
// POWER RESTORED (SRS 13.5). Tidak melakukan apa pun selain
// mencatat log dan langsung lanjut ke INITIALIZE.
// ----------------------------------------------------------------
=======
// BOOT: titik masuk tunggal, baik dari cold start maupun dari
// POWER RESTORED (SRS 13.5). Tidak melakukan apa pun selain
// mencatat log dan langsung lanjut ke INITIALIZE.
>>>>>>> origin/main
void StateMachine::runBoot() {
  Serial.println("[BOOT] Memulai Biometric Smart Key Firmware");

  // eventLogger.begin() sudah dipanggil lebih dulu di main.cpp::setup(),
  // sebelum stateMachine.begin() dijalankan, sehingga aman dicatat di sini.
  eventLogger.logEvent(EventType::SYSTEM_BOOT, -1, "Firmware boot");

  enterState(SystemState::INITIALIZE);
}

<<<<<<< HEAD
// ----------------------------------------------------------------
// INITIALIZE: konfigurasi modul & pin (SRS 5.1). Aktuator SELALU
// dipaksa ke posisi LOCKED di sini sebagai fail-safe pertama,
// sebelum status hardware diverifikasi.
// ----------------------------------------------------------------
=======
// INITIALIZE: konfigurasi modul & pin (SRS 5.1). Aktuator SELALU
// dipaksa ke posisi LOCKED di sini sebagai fail-safe pertama,
// sebelum status hardware diverifikasi.
>>>>>>> origin/main
void StateMachine::runInitialize() {
  setIndicator(false, false, true); // biru = sedang inisialisasi

  fingerprintModule.begin();
  actuatorModule.begin(); // fail-safe: langsung LOCKED di sini

  enterState(SystemState::HARDWARE_CHECK);
}

<<<<<<< HEAD
// ----------------------------------------------------------------
=======
>>>>>>> origin/main
// HARDWARE_CHECK: verifikasi sensor & aktuator sebelum sistem
// diizinkan masuk ke mode operasional (SRS 5.1).
//   PASS -> LOCKED -> IDLE
//   FAIL -> ERROR_SAFE
<<<<<<< HEAD
// ----------------------------------------------------------------
=======
>>>>>>> origin/main
void StateMachine::runHardwareCheck() {
  bool sensorOk   = fingerprintModule.healthCheck();
  bool actuatorOk = actuatorModule.healthCheck();

  if (sensorOk && actuatorOk) {
    Serial.println("[HARDWARE_CHECK] PASS");
    enterState(SystemState::LOCKED);
  } else {
    Serial.print("[HARDWARE_CHECK] FAIL (sensor=");
    Serial.print(sensorOk ? "OK" : "FAIL");
    Serial.print(", actuator=");
    Serial.print(actuatorOk ? "OK" : "FAIL");
    Serial.println(")");

    char msg[28];
    snprintf(msg, sizeof(msg), "sensor=%s actuator=%s",
             sensorOk ? "OK" : "FAIL", actuatorOk ? "OK" : "FAIL");
    eventLogger.logEvent(EventType::HARDWARE_CHECK_FAIL, -1, msg);

    enterState(SystemState::ERROR_SAFE);
  }
}

<<<<<<< HEAD
// ----------------------------------------------------------------
// LOCKED: kondisi aktuator terkunci secara fisik, transit singkat
// menuju IDLE begitu dikonfirmasi terkunci (SRS 6.1).
// ----------------------------------------------------------------
=======
// LOCKED: kondisi aktuator terkunci secara fisik, transit singkat
// menuju IDLE begitu dikonfirmasi terkunci (SRS 6.1).
>>>>>>> origin/main
void StateMachine::runLocked() {
  actuatorModule.lock();
  setIndicator(false, false, true); // biru = aman/terkunci
  enterState(SystemState::IDLE);
}

<<<<<<< HEAD
// ----------------------------------------------------------------
// IDLE: menunggu sentuhan jari secara non-blocking (SRS 6.1).
// ----------------------------------------------------------------
void StateMachine::runIdle() {
  setIndicator(false, false, true);

#if ENABLE_PHASE4_FEATURES
  // Dicek LEBIH DULU daripada sidik jari: permintaan REMOTE_UNLOCK yang
  // sudah lolos verifikasi HMAC+expiry di command_client.cpp (net task)
  // berhak langsung membuka kunci tanpa menunggu VERIFYING, persis seperti
  // operator yang menekan tombol unlock dari dashboard. Konsisten dengan
  // SRS 6.3: hanya berlaku di IDLE -- TIDAK PERNAH dikonsumsi saat
  // ERROR_SAFE/VERIFYING/dsb, karena fungsi ini memang hanya dipanggil
  // dari sini. Command yang datang saat device tidak di IDLE akan
  // kadaluwarsa sendiri di sisi command_client (checkUnlockBridgeExpiry())
  // dan dilaporkan "expired" ke backend, bukan menggantung selamanya.
  char remoteCmdId[COMMAND_ID_MAX_LEN];
  if (commandClient.consumeUnlockRequest(remoteCmdId, sizeof(remoteCmdId))) {
    _remoteUnlockActive = true;
    _remoteCommandId = remoteCmdId;
    enterState(SystemState::UNLOCKED);
    return;
  }
#endif

=======
// IDLE: menunggu sentuhan jari secara non-blocking (SRS 6.1).
void StateMachine::runIdle() {
  setIndicator(false, false, true);

>>>>>>> origin/main
  if (fingerprintModule.isFingerPresent()) {
    enterState(SystemState::VERIFYING);
  }
}

<<<<<<< HEAD
// ----------------------------------------------------------------
// VERIFYING: proses pencocokan sidik jari (SRS 5.2, FR-001, FR-003).
// verify() sendiri sudah dibatasi FP_VERIFY_TIMEOUT_MS agar tidak
// memblokir loop() terlalu lama.
// ----------------------------------------------------------------
=======
// VERIFYING: proses pencocokan sidik jari (SRS 5.2, FR-001, FR-003).
// verify() sendiri sudah dibatasi FP_VERIFY_TIMEOUT_MS agar tidak
// memblokir loop() terlalu lama.
>>>>>>> origin/main
void StateMachine::runVerifying() {
  setIndicator(false, false, true);
  Serial.println("[VERIFYING] Mencocokkan sidik jari...");

  FingerprintResult result = fingerprintModule.verify();

  switch (result) {
    case FingerprintResult::MATCH:
      enterState(SystemState::UNLOCKED);
      break;

    case FingerprintResult::NO_MATCH:
      enterState(SystemState::DENIED);
      break;

    case FingerprintResult::SENSOR_ERROR:
      Serial.println("[VERIFYING] Sensor error -> ERROR_SAFE");
      enterState(SystemState::ERROR_SAFE);
      break;

    case FingerprintResult::NO_FINGER:
    default:
      // Jari diangkat sebelum proses selesai; kembali ke IDLE.
      enterState(SystemState::IDLE);
      break;
  }
}

<<<<<<< HEAD
// ----------------------------------------------------------------
// UNLOCKED: aktuator terbuka, indikator hijau, auto-lock setelah
// UNLOCK_DURATION_MS (SRS 5.3, FR-004, FR-005).
// ----------------------------------------------------------------
=======
// UNLOCKED: aktuator terbuka, indikator hijau, auto-lock setelah
// UNLOCK_DURATION_MS (SRS 5.3, FR-004, FR-005).
>>>>>>> origin/main
void StateMachine::runUnlocked() {
  // Aksi buka hanya dijalankan sekali saat baru masuk state ini.
  if (timeInState() < 20) { // window kecil untuk "baru saja masuk"
    actuatorModule.unlock();
    setIndicator(false, true, false); // hijau
<<<<<<< HEAD

#if ENABLE_PHASE4_FEATURES
    if (_remoteUnlockActive) {
      Serial.println("[UNLOCKED] Akses diberikan via REMOTE_UNLOCK (backend).");
      eventLogger.logEvent(EventType::REMOTE_UNLOCK, -1, "Remote unlock command executed");
      // Laporkan hasil SEKARANG (bukan tunda) -- commandClient yang akan
      // mengirim ack ke backend dari net task saat tick berikutnya.
      commandClient.reportUnlockResult(_remoteCommandId.c_str(), true, "unlocked");
      _remoteUnlockActive = false;
      _remoteCommandId = "";
    } else
#endif
    {
      Serial.println("[UNLOCKED] Akses diberikan. Auto-lock dalam "
                      + String(UNLOCK_DURATION_MS / 1000) + " detik.");
      eventLogger.logEvent(EventType::ACCESS_GRANTED,
                            static_cast<int16_t>(fingerprintModule.lastMatchedSlotId()),
                            "Fingerprint matched");
    }
=======
    Serial.println("[UNLOCKED] Akses diberikan. Auto-lock dalam "
                    + String(UNLOCK_DURATION_MS / 1000) + " detik.");

    eventLogger.logEvent(EventType::ACCESS_GRANTED,
                          static_cast<int16_t>(fingerprintModule.lastMatchedSlotId()),
                          "Fingerprint matched");
>>>>>>> origin/main
  }

  if (timeInState() >= UNLOCK_DURATION_MS) {
    enterState(SystemState::LOCKED); // -> LOCKED -> IDLE (lihat runLocked)
  }
}

<<<<<<< HEAD
// ----------------------------------------------------------------
// DENIED: akses ditolak, indikator merah sesaat, lalu kembali IDLE
// (SRS 6.1, FR-003).
// ----------------------------------------------------------------
=======
// DENIED: akses ditolak, indikator merah sesaat, lalu kembali IDLE
// (SRS 6.1, FR-003).
>>>>>>> origin/main
void StateMachine::runDenied() {
  if (timeInState() < 20) {
    setIndicator(true, false, false); // merah
    Serial.println("[DENIED] Sidik jari tidak dikenali.");

    eventLogger.logEvent(EventType::ACCESS_DENIED, -1, "Fingerprint not recognized");
  }

  if (timeInState() >= DENIED_DISPLAY_MS) {
    enterState(SystemState::IDLE);
  }
}

<<<<<<< HEAD
// ----------------------------------------------------------------
// ERROR_SAFE: pintu dipaksa terkunci secara mekanis, perintah buka
// dilarang keras (SRS 6.3), lalu menjalankan siklus recovery
// menuju HARDWARE_CHECK setelah jeda (SRS 5.4, 13.3, 13.7).
// ----------------------------------------------------------------
=======
// ERROR_SAFE: pintu dipaksa terkunci secara mekanis, perintah buka
// dilarang keras (SRS 6.3), lalu menjalankan siklus recovery
// menuju HARDWARE_CHECK setelah jeda (SRS 5.4, 13.3, 13.7).
>>>>>>> origin/main
void StateMachine::runErrorSafe() {
  if (timeInState() < 20) {
    actuatorModule.lock(); // paksa terkunci, tanpa pengecualian
    setIndicator(true, false, false);
    Serial.println("[ERROR_SAFE] Gangguan perangkat keras terdeteksi. "
                    "Pintu dipaksa terkunci. Memulai recovery...");

    eventLogger.logEvent(EventType::ERROR_SAFE_TRIGGERED, -1, "Forced lock, anomaly detected");
  }

  // Blink merah sebagai indikator visual bahwa sistem dalam
  // recovery (tidak memblokir loop, hanya membaca waktu).
  bool blinkOn = (millis() / 300) % 2 == 0;
  setIndicator(blinkOn, false, false);

  if (timeInState() >= ERROR_RECOVERY_DELAY_MS) {
    Serial.println("[ERROR_SAFE] Mencoba kembali HARDWARE_CHECK...");
    eventLogger.logEvent(EventType::RECOVERY_ATTEMPT, -1, "Retrying hardware check");
    enterState(SystemState::HARDWARE_CHECK);
  }
}
