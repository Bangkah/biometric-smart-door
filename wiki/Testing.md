# Testing

Strategi test dua bahasa (C++ firmware, Python backend), dijalankan
sebagai quality gate wajib di CI sebelum PR bisa direview.

## Firmware (Native Unit Test, Host — Tanpa ESP32)

```bash
pio test -e native
```

Prinsip: logika yang **bisa** diuji murni (tanpa Wi-Fi/HTTP/hardware
sungguhan) diekstrak ke modul terpisah, lalu diuji sebagai kode PRODUKSI
(bukan tiruan) jika memungkinkan.

| Suite | Menguji | Pendekatan |
|---|---|---|
| `test_backoff` | `backoff.h` | Pure function |
| `test_time_estimate` | `time_estimate.h` | Pure function |
| `test_url_utils` | `url_utils.h` | Pure function |
| `test_command_policy` | `phase4/command_policy.h/.cpp` | Pure function; vektor uji **identik** dengan `backend/tests/test_security.py` (silang-verifikasi C++ ⟷ Python) |
| `test_event_logger` | `event_logger.cpp` (PRODUKSI) | LittleFS/Preferences/FreeRTOS **palsu tapi fungsional** (`test/stubs/`), reboot disimulasikan dengan instance baru di atas "flash" yang sama |
| `test_access_policy` | `phase4/access_policy.cpp` (PRODUKSI) | Pola sama (Preferences fungsional), menguji persistensi revocation lintas reboot |

**Tidak** diuji di `env:native`: `network_manager.cpp`, `time_sync.cpp`,
`api_client.cpp`, `command_client.cpp` — semuanya meng-`#include` header
Wi-Fi/HTTP/TLS/mbedtls asli yang tak tersedia di host. Logika
perhitungannya sudah diekstrak ke modul pure di atas; sisanya divalidasi
lewat **build** (bagian berikut) dan bench-test manual
([[Firmware-Setup]], [[Remote-Commands]]).

### Build Matrix (Compile Check)

```bash
pio run -e esp32dev              # produksi
pio run -e esp32dev-bench        # + HTTP lokal untuk mock backend
pio run -e esp32dev-phase3only   # TANPA folder src/phase4/ — bukti batas fase
```

`esp32dev-phase3only` bukan sekadar nama environment — `build_src_filter`
di `platformio.ini` benar-benar mengeluarkan `src/phase4/` dari
kompilasi, dan `ENABLE_PHASE4_FEATURES=0` memastikan tidak ada kode Phase
1-3 yang mencoba memanggil API Phase 4. Jika build ini gagal, berarti ada
pelanggaran arah dependensi (lihat [[Architecture]]).

## Backend (pytest)

```bash
cd backend
pip install -r requirements-dev.txt
pytest -v --cov=app --cov-report=term-missing
ruff check .
```

| File test | Fokus |
|---|---|
| `test_events.py` | Ingestion, idempotensi, auth, dedup lintas-epoch |
| `test_devices.py` | Registrasi device, status online/offline |
| `test_heartbeat.py` | Heartbeat independen dari event sync |
| `test_commands.py` | Create/poll/ack command, **verifikasi signature HMAC**, idempotensi ack, expiry |
| `test_users.py` | Enroll/revoke kredensial, auto-command `ALLOW_SLOT`/`REVOKE_SLOT` |
| `test_security.py` | Fungsi murni: `canonical_command_payload`, `sign_command`, `is_timestamp_fresh` — vektor uji dibagi dengan firmware |
| `test_payload_validation.py` | Payload tidak valid (tipe salah, field hilang, enum salah) ditolak 422 |
| `test_phase_boundary.py` | `PHASE4_ENABLED=false` benar-benar meniadakan router (404, bukan 401) |

Coverage saat ini **~98%**, gate CI mensyaratkan minimum **90%**
(`--cov-fail-under=90`) — longgar sengaja supaya penambahan kecil tidak
tiba-tiba menggagalkan pipeline, tapi tetap menolak penurunan signifikan.

## CI/CD (`.github/workflows/firmware-ci.yml`)

Tiga lapis, berurutan (fail-fast):

```
1a. firmware-unit-tests (pio test -e native)  ─┐
1b. backend-tests (pytest + ruff)              ├─ PARALEL, keduanya cepat
                                                ┘
2.  firmware-build (matrix: esp32dev / esp32dev-bench / esp32dev-phase3only)
    -- hanya jalan jika 1a DAN 1b lulus --
    + pio check --fail-on-defect=high (static analysis, MENGGAGALKAN pipeline)

quality-gate (ringkasan semua job di atas, untuk branch protection rule)
```

Trigger: setiap PR ke `main`, setiap push ke branch `phase-*`. Lihat
[[Contributing]] untuk strategi branching lengkap.

## Menambah Test Baru

**Firmware** — logika baru yang bisa diuji murni → ekstrak ke header/`.cpp`
tanpa `#include <Arduino.h>` dkk (pola `backoff.h`), atau jika perlu
menguji `.cpp` yang sudah memakai Arduino/LittleFS/Preferences (pola
`event_logger.cpp`), pastikan stub fungsional relevan ada di
`test/stubs/`. Buat folder `test/test_<nama>/test_<nama>.cpp` baru, lalu
tambahkan ke `build_src_filter` di `[env:native]` bila perlu file `.cpp`
tambahan dari `src/`.

**Backend** — tambah file `backend/tests/test_<nama>.py`, pakai fixture
`client`/`client_phase4_disabled`/`admin_headers`/`registered_device` dari
`conftest.py`.
