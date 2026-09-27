# Biometric Smart Door

Sistem kontrol akses pintu berbasis biometrik (sidik jari) dengan arsitektur **Decoupled Edge–Backend**: perangkat edge (ESP32) mengambil keputusan buka/tutup secara mandiri (*local-first*), sementara backend dan web dashboard menangani manajemen perangkat, pengguna, audit log, dan remote unlock.

> Spesifikasi lengkap ada di [`docs/srs.md`](docs/srs.md).

## Fitur Utama

- Verifikasi sidik jari lokal (< 2 detik), tanpa bergantung pada koneksi jaringan
- Web dashboard untuk manajemen pengguna, perangkat, dan enrollment
- Audit trail lengkap untuk seluruh akses & perintah administratif
- Remote unlock dengan proteksi replay & eksekusi idempotent (`command_id`, `issued_at`, `expires_at`)
- Persistent event buffer di perangkat — log tetap tersimpan saat offline dan disinkronkan otomatis saat online kembali
- Fail-safe: kegagalan sistem apa pun membuat pintu kembali ke kondisi **terkunci**
- Privacy by design — data biometrik mentah tidak pernah dikirim/disimpan di server

## Arsitektur

```
┌─────────────┐        HTTPS         ┌─────────────┐        HTTPS        ┌──────────────┐
│  Edge Device │ ───────────────────▶ │   Backend    │ ◀─────────────────▶ │ Web Dashboard│
│   (ESP32)    │  heartbeat, events   │  REST API +  │      admin ops       │   (Admin)    │
│  + Sensor    │ ◀─────────────────── │   Database   │                     │              │
│  + Actuator  │  command queue       └─────────────┘                     └──────────────┘
└─────────────┘
```

- **Edge Device** — ESP32 + sensor sidik jari (AS608/R307) + solenoid/servo lock, menjalankan state machine (`BOOT → INITIALIZE → HARDWARE_CHECK → LOCKED/IDLE → VERIFYING → UNLOCKED/DENIED`, dengan `ERROR_SAFE` untuk kegagalan).
- **Backend** — REST API untuk autentikasi perangkat & admin, sinkronisasi kredensial, penerimaan log, dan antrean command.
- **Database** — Relasional, menyimpan `users`, `fingerprint_credentials`, `devices`, `access_logs`, `commands`.
- **Web Dashboard** — Panel admin: login, ringkasan status, manajemen user/device, enrollment, access log, remote unlock.

## Tech Stack

| Layer | Teknologi |
|---|---|
| Firmware | ESP32 (Arduino IDE / ESP-IDF), sensor UART (AS608/R307) |
| Backend | REST API (TBD), Database relasional (TBD) |
| Frontend | Web Dashboard (TBD) |
| Simulasi | Wokwi / push-button + serial trigger |
| Deployment | Docker / cloud instance, HTTPS |


Struktur akan bertambah seiring implementasi, mengikuti fase berikut:

1. **Hardware Prototype** — ESP32 + sensor + servo/relay + state machine (tanpa backend)
2. **Local Access Control** — verifikasi lokal, hardware check, error-safe, persistent event buffer
3. **Backend & Device Communication** — autentikasi perangkat, sinkronisasi event
4. **Web Dashboard** — audit log & status perangkat
5. **Enrollment & Credential Management** — siklus hidup pendaftaran sidik jari + revocation
6. **Remote Unlock** — kontrol jarak jauh dengan proteksi replay & idempotency
7. **Security, Testing & Final Validation** — TLS, hardening, pengujian kegagalan

## Keamanan

- Seluruh komunikasi edge ⇄ backend ⇄ dashboard menggunakan **TLS/HTTPS**
- Password admin di-hash dengan **Bcrypt/Argon2**
- Setiap perangkat memiliki **token identitas unik**
- Command remote unlock wajib **idempotent** dan divalidasi masa berlakunya (`expires_at`)
- Data biometrik mentah **tidak pernah** meninggalkan perangkat edge

## Status Proyek

Dalam pengembangan — lihat [SRS](docs/srs.md) untuk detail requirement, database schema, API, dan acceptance criteria.

## Progres & Status Implementasi Terkini

Proyek ini dikembangkan secara bertahap menggunakan pendekatan ketat berbasis *strict CI/CD quality gate* (GitHub Actions dengan `-Werror` dan analisis statis Cppcheck), meskipun diuji melalui simulasi perangkat lunak (*mock mode*).

* **[Phase 1: Hardware Prototype & State Machine](https://github.com/Bangkah/biometric-smart-door/tree/phase-1)** ➔ **[SELESAI - Quality Gate Aktif]**
  * Perancangan *state machine* non-blocking (`BOOT`, `INITIALIZE`, `HARDWARE_CHECK`, `LOCKED`, `IDLE`, `VERIFYING`, `UNLOCKED`, `DENIED`, `ERROR_SAFE`).
  * Modul abstrak sensor sidik jari dengan *Mock Mode* penuh agar dapat diuji tanpa perangkat keras fisik via Serial Monitor (`t`/`m`/`x`).
  * Integrasi Watchdog Timer (8 detik) untuk pemulihan mandiri.
  * *CI/CD:* Terverifikasi otomatis via `.github/workflows/phase1-ci.yml`.

* **[Phase 2: Local Access Control & Persistent Event Buffer](https://github.com/Bangkah/biometric-smart-door/tree/phase-2)** ➔ **[SELESAI - Quality Gate Aktif]**
  * Pencatatan log lokal persisten menggunakan **LittleFS** berbasis *ring buffer* (50 entri).
  * Tahan terhadap pemadaman listrik (*power-loss resilience*) dengan *self-healing sequence recovery* saat *boot*.
  * Debugger Serial interaktif untuk manajemen log (`l` untuk melihat log, `c` untuk *clear log*).
  * *CI/CD:* Terverifikasi otomatis via pipeline CI.

* **Phase 3: Network & Backend Integration** ➔ **[BERIKUTNYA]**
  * Wi-Fi auto-reconnect, NTP time synchronization, dan sinkronisasi *payload* event log ke backend server.

## Lisensi

Belum ditentukan.