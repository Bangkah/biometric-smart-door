#include "actuator.h"
#include "config.h"

#ifdef ACTUATOR_TYPE_SERVO
#include <ESP32Servo.h>
static Servo doorServo;
#endif

ActuatorModule actuatorModule;

void ActuatorModule::begin() {
#ifdef ACTUATOR_TYPE_SERVO
  doorServo.attach(SERVO_PIN);
#else
  pinMode(RELAY_PIN, OUTPUT);
#endif

  // Fail-safe: paksa terkunci saat inisialisasi, apa pun state
  // sebelumnya (SRS 5.1 Boot & Initialization, 13.5 Power Loss & Recovery).
  lock();
}

void ActuatorModule::writeRelay(bool energize) {
#if RELAY_ACTIVE_LOW
  digitalWrite(RELAY_PIN, energize ? LOW : HIGH);
#else
  digitalWrite(RELAY_PIN, energize ? HIGH : LOW);
#endif
}

bool ActuatorModule::healthCheck() {
  // Phase 1 bench test: pastikan aktuator dapat menerima perintah
  // lock tanpa exception/hang. Untuk hardware dengan sensor posisi
  // (limit switch), tambahkan pembacaan aktual di sini dan
  // kembalikan false bila posisi tidak sesuai perintah.
  lock();
  return true;
}

void ActuatorModule::unlock() {
#ifdef ACTUATOR_TYPE_SERVO
  doorServo.write(SERVO_UNLOCKED_ANGLE);
#else
  writeRelay(true);
#endif
  _locked = false;
  Serial.println("[ACTUATOR] -> UNLOCKED");
}

void ActuatorModule::lock() {
#ifdef ACTUATOR_TYPE_SERVO
  doorServo.write(SERVO_LOCKED_ANGLE);
#else
  writeRelay(false);
#endif
  _locked = true;
  Serial.println("[ACTUATOR] -> LOCKED");
}
