#pragma once
/*
 * state_machine.h
 * ------------------------------------------------------------
 * Implementasi state machine firmware sesuai SRS Bab 6.
 *
 * States:
 *   BOOT -> INITIALIZE -> HARDWARE_CHECK
 *     PASS -> LOCKED -> IDLE
 *     FAIL -> ERROR_SAFE
 *   IDLE + jari terdeteksi -> VERIFYING
 *   VERIFYING + match -> UNLOCKED -> (timeout) -> LOCKED -> IDLE
 *   VERIFYING + no match -> DENIED -> IDLE
 *   Anomali HW -> ERROR_SAFE -> recovery -> HARDWARE_CHECK
 *   POWER RESTORED -> BOOT -> INITIALIZE -> HARDWARE_CHECK
 *
 * update() dipanggil setiap loop() dan bersifat non-blocking
 * (state timing berbasis millis(), bukan delay()) agar watchdog
 * di main.cpp tetap bisa di-reset secara berkala.
 * ------------------------------------------------------------
 */

#include <Arduino.h>

enum class SystemState {
  BOOT,
  INITIALIZE,
  HARDWARE_CHECK,
  LOCKED,
  IDLE,
  VERIFYING,
  UNLOCKED,
  DENIED,
  ERROR_SAFE
};

const char* stateToString(SystemState state);

class StateMachine {
  public:
    void begin();
    void update();

    SystemState getState() const { return _state; }

  private:
    SystemState _state = SystemState::BOOT;
    unsigned long _stateEnteredAt = 0;

    void enterState(SystemState newState);
    unsigned long timeInState() const { return millis() - _stateEnteredAt; }

    void runBoot();
    void runInitialize();
    void runHardwareCheck();
    void runLocked();
    void runIdle();
    void runVerifying();
    void runUnlocked();
    void runDenied();
    void runErrorSafe();

    void setIndicator(bool red, bool green, bool blue);
<<<<<<< HEAD

    // [Phase 4] Menandai UNLOCKED saat ini berasal dari command REMOTE_UNLOCK
    // (lewat CommandClient::consumeUnlockRequest di runIdle()), bukan sidik
    // jari -- menentukan EventType yang dicatat & apakah hasilnya perlu
    // dilaporkan balik ke commandClient (lihat runUnlocked()). Selalu ada di
    // header (tidak dipagari #if) karena hanya String+bool biasa, tidak
    // butuh apa pun dari src/phase4/ -- menjaga state_machine.h bebas
    // dependensi Phase 4 sesuai arah ketergantungan di wiki/Architecture.md.
    bool _remoteUnlockActive = false;
    String _remoteCommandId;
=======
>>>>>>> origin/main
};

extern StateMachine stateMachine;
