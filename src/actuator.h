#pragma once
/*
 * actuator.h
 * ------------------------------------------------------------
 * Abstraksi aktuator pengunci mekanis (SRS 4.3, 13.4).
 * Mendukung Relay+Solenoid (default) atau Servo (prototipe),
 * dipilih lewat ACTUATOR_TYPE_SERVO di config.h.
 *
 * Fail-safe: konstruktor/begin() SELALU menempatkan aktuator
 * pada posisi LOCKED terlebih dahulu (SRS 2.2 Fail-Safe Security).
 * ------------------------------------------------------------
 */

#include <Arduino.h>

class ActuatorModule {
  public:
    void begin();

    // Pemeriksaan kesehatan aktuator saat HARDWARE_CHECK.
    // Pada implementasi nyata, ini bisa membaca sensor posisi
    // (reed switch/limit switch) jika tersedia. Untuk Phase 1
    // (prototipe bangku), ini memverifikasi aktuator merespons
    // perintah lock secara mekanis tanpa error.
    bool healthCheck();

    // Menggerakkan aktuator ke posisi terbuka.
    void unlock();

    // Memaksa aktuator ke posisi terkunci (dipanggil di semua
    // jalur error dan saat auto-lock timeout).
    void lock();

    bool isLocked() const { return _locked; }

  private:
    bool _locked = true;
    void writeRelay(bool energize);
};

extern ActuatorModule actuatorModule;
