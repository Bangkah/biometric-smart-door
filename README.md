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
┌──────────────┐  heartbeat, events  ┌──────────────┐    admin ops    ┌───────────────┐
│ Edge Device  │ ──────────────────▶ │   Backend    │ ◀─────────────▶ │ Web Dashboard │
│   (ESP32)    │ ◀────────────────── │ REST API + DB│                 │    (Admin)    │
│ Sensor+Lock  │    command queue    └──────────────┘                 └───────────────┘
└──────────────┘
```

- **Edge Device** — ESP32 + sensor sidik jari (AS608/R307) + solenoid/servo lock, menjalankan state machine (`BOOT → INITIALIZE → HARDWARE_CHECK → LOCKED/IDLE → VERIFYING → UNLOCKED/DENIED`, dengan `ERROR_SAFE` untuk kegagalan).
- **Backend** — REST API untuk autentikasi perangkat & admin, sinkronisasi kredensial, penerimaan log, dan antrean command.
- **Database** — Relasional, menyimpan `users`, `fingerprint_credentials`, `devices`, `access_logs`, `commands`.
- **Web Dashboard** — Panel admin: login, ringkasan status, manajemen user/device, enrollment, access log, remote unlock.

## Tech Stack

| Layer | Teknologi |
|---|---|
| Firmware | ESP32 (Arduino IDE / ESP-IDF), sensor UART (AS608/R307) |
| Backend | Node.js (Express.js / TypeScript), Prisma ORM |
| Database | PostgreSQL |
| Frontend | React.js (Vite, Tailwind CSS) |
| Simulasi & Testing | Mock Mode (Serial Monitor / Wokwi) & GitHub Actions CI/CD |
| Deployment | Docker / cloud instance, HTTPS |

## Keamanan

- Seluruh komunikasi edge ⇄ backend ⇄ dashboard menggunakan **TLS/HTTPS**
- Password admin di-hash dengan **Bcrypt/Argon2**
- Setiap perangkat memiliki **token identitas unik**
- Command remote unlock wajib **idempotent** dan divalidasi masa berlakunya (`expires_at`)
- Data biometrik mentah **tidak pernah** meninggalkan perangkat edge

## Roadmap & Status Implementasi

Proyek dikembangkan bertahap dengan *strict CI/CD quality gate* (GitHub Actions dengan `-Werror` dan analisis statis Cppcheck), meskipun saat ini diuji melalui simulasi perangkat lunak (*mock mode*).

| # | Fase | Status |
|---|---|---|
| 1 | Hardware Prototype & State Machine | ✅ Selesai |
| 2 | Local Access Control & Persistent Event Buffer | ✅ Selesai |
| 3 | Network & Backend Integration | ⏭️ Berikutnya |
| 4 | Web Dashboard | ⏳ Direncanakan |
| 5 | Enrollment & Credential Management | ⏳ Direncanakan |
| 6 | Remote Unlock | ⏳ Direncanakan |
| 7 | Security, Testing & Final Validation | ⏳ Direncanakan |

### [Phase 1](https://github.com/Bangkah/biometric-smart-door/tree/phase-1) — Hardware Prototype & State Machine

- *State machine* non-blocking (`BOOT`, `INITIALIZE`, `HARDWARE_CHECK`, `LOCKED`, `IDLE`, `VERIFYING`, `UNLOCKED`, `DENIED`, `ERROR_SAFE`).
- Modul abstrak sensor sidik jari dengan *Mock Mode* penuh, dapat diuji tanpa perangkat keras fisik via Serial Monitor (`t` / `m` / `x`).
- Watchdog Timer (8 detik) untuk pemulihan mandiri.
- CI/CD: `.github/workflows/phase1-ci.yml`.

### [Phase 2](https://github.com/Bangkah/biometric-smart-door/tree/phase-2) — Local Access Control & Persistent Event Buffer

- Log lokal persisten memakai **LittleFS** berbasis *ring buffer* (50 entri).
- Tahan pemadaman listrik (*power-loss resilience*) dengan *self-healing sequence recovery* saat boot.
- Debugger Serial interaktif untuk manajemen log (`l` lihat log, `c` hapus log).
- CI/CD: terverifikasi otomatis via pipeline CI.

### Phase 3 — Network & Backend Integration *(berikutnya)*

- Wi-Fi auto-reconnect, sinkronisasi waktu via NTP, dan sinkronisasi *payload* event log ke backend.

## Lisensi

Belum ditentukan.