# Backend Setup

Backend FastAPI untuk ingestion log, manajemen perangkat/pengguna, dan
command queue (SRS Bab 8-12, Phase 3 & 4).

## Kebutuhan

- Python 3.11+
- SQLite (bawaan Python, cukup untuk pengembangan & bench-test) — atau
  PostgreSQL untuk produksi (ganti `DATABASE_URL`, lihat di bawah)

## Instalasi & Menjalankan Lokal

```bash
cd backend
python -m venv .venv && source .venv/bin/activate   # opsional tapi disarankan
pip install -r requirements-dev.txt

cp .env.example .env
# Edit .env, WAJIB isi ADMIN_API_KEY dengan nilai acak:
#   openssl rand -hex 32

uvicorn app.main:app --reload --port 8000
```

Buka `http://localhost:8000` — dashboard statis akan muncul, minta
`X-Admin-Key` (isi dengan `ADMIN_API_KEY` dari `.env`). Dokumentasi API
interaktif (Swagger UI) otomatis tersedia di
`http://localhost:8000/docs`.

## Konfigurasi (`.env`)

| Variabel | Default | Keterangan |
|---|---|---|
| `ADMIN_API_KEY` | *(wajib diisi)* | Kunci header `X-Admin-Key` untuk seluruh endpoint admin/dashboard. Proses **gagal start** tanpa ini (fail-closed) |
| `DATABASE_URL` | `sqlite:///./biometric_smart_key.db` | SQLAlchemy connection string. Produksi: `postgresql+psycopg://user:pass@host/db` |
| `COMMAND_FRESHNESS_WINDOW_SEC` | `120` | Jendela toleransi waktu untuk validasi timestamp (anti-replay kasar) |
| `PHASE4_ENABLED` | `true` | Toggle router `users`/`commands`. `false` = backend "Phase 3 murni", lihat [[Architecture]] |

## Struktur Kode

```
backend/
├── app/
│   ├── main.py              # create_app() factory, lifespan, static dashboard
│   ├── config.py             # Settings dari environment variable
│   ├── database.py           # SQLAlchemy engine/session
│   ├── models.py              # ORM: User, FingerprintCredential, Device, AccessLog, Command
│   ├── schemas.py              # Pydantic request/response
│   ├── security.py              # Hashing token, HMAC signing, dependency auth
│   ├── crud.py                   # Operasi DB bersama (heartbeat, expire command, dst.)
│   ├── routers/
│   │   ├── phase3/                # events, devices, heartbeat — SELALU aktif
│   │   └── phase4/                # users, commands — toggle PHASE4_ENABLED
│   └── static/dashboard.html        # Dashboard satu-file (vanilla JS, tanpa build step)
├── tests/                     # pytest (lihat [[Testing]])
├── requirements.txt / requirements-dev.txt
├── pytest.ini / ruff.toml
└── .env.example
```

## Model Data (Ringkas)

| Tabel | Kolom kunci |
|---|---|
| `devices` | `id`, `api_token_hash` (SHA-256, bukan plaintext), `hmac_secret`, `last_seen`, `firmware_version`, `last_log_epoch` |
| `users` | `id`, `name`, `status` (active/revoked) |
| `fingerprint_credentials` | `user_id`, `device_id`, `sensor_slot_id`, `status`, unique `(device_id, sensor_slot_id)` |
| `access_logs` | `device_id`, `log_epoch`, `boot_id`, `sequence` (unique bersama = idempotensi), `event_type`, `timestamp`, `time_source` |
| `commands` | `device_id`, `command_type`, `payload` (JSON), `status`, `issued_at`, `expires_at` |

Detail endpoint lengkap: [[API-Reference]]. Model keamanan: [[Security]].

## Mendaftarkan Perangkat Pertama

```bash
curl -X POST http://localhost:8000/api/v1/devices \
  -H "X-Admin-Key: $ADMIN_API_KEY" -H "Content-Type: application/json" \
  -d '{"id": "DOOR-01", "name": "Pintu Depan", "location": "Lobby"}'
```

Response berisi `api_token` dan `hmac_secret` **hanya saat ini** (tidak
pernah bisa diambil ulang — backend hanya menyimpan hash-nya). Salin
keduanya ke `include/secrets.h` firmware (lihat [[Firmware-Setup]]).

## Produksi

- Ganti `DATABASE_URL` ke PostgreSQL untuk konkurensi yang lebih baik.
- Jalankan di belakang reverse proxy (nginx/Caddy) yang menangani TLS —
  atau terminasi TLS langsung di Uvicorn dengan `--ssl-keyfile`/`--ssl-certfile`.
- `ADMIN_API_KEY` **wajib** string acak panjang (`openssl rand -hex 32`),
  bukan nilai contoh di `.env.example`.
- Pertimbangkan migrasi skema (Alembic) untuk perubahan model di masa
  depan — Phase 4 masih memakai `Base.metadata.create_all()` (cukup untuk
  skala proyek saat ini, SQLite/dev).
