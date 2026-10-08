# Biometric Smart Key & Management System

Sistem kontrol akses biometrik berbasis ESP32 dengan backend manajemen
jarak jauh — dibangun bertahap sesuai SRS, dari prototipe hardware murni
offline hingga dashboard dengan command queue dan sinkronisasi akses
over-the-air.

**Edge Autonomy** adalah prinsip inti: pintu tetap terbuka/terkunci sesuai
sidik jari lokal walau Wi-Fi dan backend mati total. Jaringan hanya
menambah kemampuan (telemetry, manajemen jarak jauh) — tidak pernah
menjadi prasyarat fungsi dasar.

```
┌──────────────┐   HTTPS (Bearer + HMAC)   ┌──────────────────┐
│  ESP32 Edge   │ ◄───────────────────────► │  Backend FastAPI  │
│  (firmware)   │   events · heartbeat ·    │  + Dashboard web   │
│               │   commands · ack          │  + SQLite/Postgres │
└──────────────┘                           └──────────────────┘
```

Diagram & penjelasan arsitektur lengkap: **[wiki/Architecture](wiki/Architecture.md)**.

## Status Fase

| Fase | Judul | Status |
|---|---|---|
| 1 | Hardware Prototype | ✅ |
| 2 | Local Event System & Memory Buffer | ✅ |
| 3 | Network, NTP & Backend Event Upload | ✅ |
| 4 | Dashboard & Manajemen Akses Jarak Jauh | ✅ |
| 5-6 | Enrollment penuh & hardening lanjutan | 🔜 |

## Mulai Cepat

**Firmware** (bench-test, tanpa sensor/backend sungguhan dulu):
```bash
cp include/secrets.h.example include/secrets.h
pio run -e esp32dev-bench --target upload
pio device monitor -b 115200
```

**Backend**:
```bash
cd backend
cp .env.example .env   # isi ADMIN_API_KEY (openssl rand -hex 32)
pip install -r requirements-dev.txt
uvicorn app.main:app --reload --port 8000
# http://localhost:8000 -> dashboard
```

**Simulasi end-to-end tanpa backend sungguhan** (events + heartbeat +
remote command):
```bash
python tools/mock_backend.py --token dev-token-123 --hmac-secret dev-hmac-456
python tools/mock_backend.py --enqueue REMOTE_UNLOCK --device DOOR-01
```

Panduan lengkap langkah-demi-langkah: **[wiki/Firmware-Setup](wiki/Firmware-Setup.md)**
dan **[wiki/Backend-Setup](wiki/Backend-Setup.md)**.

## Struktur Proyek

```
.
├── include/              # config.h (semua konfigurasi), secrets.h.example
├── src/                   # Firmware: state machine, sensor, aktuator, jaringan
│   └── phase4/             # Remote command, HMAC signing, access policy (Phase 4)
├── test/                   # Unit test native (host, tanpa ESP32) — lihat wiki/Testing
├── tools/
│   └── mock_backend.py       # Backend palsu (stdlib only) untuk bench-test lokal
├── backend/                  # Backend FastAPI (ingestion, device/user mgmt, commands)
│   ├── app/                    # routers/phase3, routers/phase4, models, security, dst.
│   └── tests/                    # pytest
├── wiki/                        # Dokumentasi teknis lengkap (lihat daftar di bawah)
└── .github/workflows/             # CI: unit test + build matrix + backend test
```

## Fitur Utama per Fase

- **Phase 1** — State machine fail-safe (SRS Bab 6), watchdog timer,
  kontrol aktuator relay/servo, mode mock tanpa sensor fisik.
- **Phase 2** — Ring buffer log persisten di LittleFS, tahan power-loss
  (self-healing dari sequence number, bukan file metadata terpisah).
- **Phase 3** — Wi-Fi + NTP non-blocking (network task terpisah dari
  loop kontrol), upload event batch idempoten, heartbeat independen,
  retry/backoff eksponensial, kebijakan TLS fail-closed.
- **Phase 4** — Command queue dengan HMAC-SHA256 per-command, remote
  unlock (bridge aman lintas-task), sinkronisasi izin akses
  (ALLOW/REVOKE_SLOT), backend FastAPI lengkap (device/user/command
  management + dashboard web), **batas fase yang ditegakkan otomatis**
  (build & test membuktikan Phase 3 tidak bergantung Phase 4).

## Keamanan

Dua kredensial terpisah per perangkat (Bearer token untuk autentikasi,
HMAC secret untuk integritas command), TLS wajib di build produksi,
verifikasi fail-closed di setiap titik keputusan. Detail & threat model
lengkap: **[wiki/Security](wiki/Security.md)**.

## Testing & CI/CD

95+ test (firmware native + backend pytest), coverage backend ~98%. CI
tiga lapis (unit test paralel → build matrix 3 environment ESP32 →
static analysis) wajib hijau sebelum PR dianggap siap. Detail:
**[wiki/Testing](wiki/Testing.md)**.

## Dokumentasi Lengkap (Wiki)

| Halaman | Isi |
|---|---|
| [Architecture](wiki/Architecture.md) | Task model, prinsip desain, batas antar-fase |
| [Firmware-Setup](wiki/Firmware-Setup.md) | Hardware, wiring, build, bench-test |
| [Remote-Commands](wiki/Remote-Commands.md) | Command queue, HMAC, remote unlock (Phase 4) |
| [Backend-Setup](wiki/Backend-Setup.md) | Menjalankan backend, konfigurasi, struktur kode |
| [API-Reference](wiki/API-Reference.md) | Seluruh endpoint REST, request/response |
| [Security](wiki/Security.md) | Model keamanan & threat model |
| [Testing](wiki/Testing.md) | Unit test, CI/CD, cara menambah test |
| [Contributing](wiki/Contributing.md) | Strategi branch per-fase, alur PR |
| [Troubleshooting](wiki/Troubleshooting.md) | Masalah umum & solusinya |
| [Final-Validation](wiki/Final-Validation.md) | Checklist Acceptance Criteria & DoD (SRS 21-22) dengan bukti test + gap register |

## Lisensi

Belum ditentukan — tambahkan file `LICENSE` sesuai kebutuhan proyek.
