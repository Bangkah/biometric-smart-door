# Biometric Smart Key & Management System — Wiki

Dokumentasi teknis lengkap untuk firmware ESP32 + backend FastAPI dari
proyek **Biometric Smart Key & Management System**, dibangun bertahap
sesuai SRS (Software Requirements Specification).

> README.md di root repo adalah ringkasan/quick-start. Wiki ini berisi
> referensi teknis lengkap per topik.

## Status Fase

| Fase | Judul | Status |
|---|---|---|
| 1 | Hardware Prototype | ✅ Selesai |
| 2 | Local Event System & Memory Buffer | ✅ Selesai |
| 3 | Network, NTP & Backend Event Upload | ✅ Selesai |
| 4 | Dashboard & Manajemen Akses Jarak Jauh | ✅ Selesai |
| 5 | Enrollment & Credential Management penuh | 🔜 Belum dimulai |
| 6 | Hardening keamanan lanjutan | 🔜 Belum dimulai |

## Peta Dokumen

| Halaman | Isi |
|---|---|
| [[Architecture]] | Arsitektur sistem, task model firmware, batas antar-fase, diagram komunikasi |
| [[Firmware-Setup]] | Hardware, wiring, build (PlatformIO/Arduino IDE), bench-test tanpa sensor fisik |
| [[Remote-Commands]] | Phase 4: command queue, HMAC signing, remote unlock, sinkronisasi akses |
| [[Backend-Setup]] | Menjalankan backend FastAPI secara lokal, konfigurasi `.env`, dashboard |
| [[API-Reference]] | Referensi lengkap seluruh endpoint REST (request/response, auth) |
| [[Security]] | Model keamanan: Bearer token, HMAC, kebijakan TLS, threat model |
| [[Testing]] | Unit test firmware (native) & backend (pytest), cakupan, CI/CD |
| [[Contributing]] | Strategi branch per-fase, alur Pull Request sebagai quality gate |
| [[Troubleshooting]] | Masalah umum & solusinya |
| [[Final-Validation]] | Checklist Acceptance Criteria & Definition of Done (SRS Bab 21-22) dengan bukti test, termasuk gap register yang jujur |

## Mulai Cepat

```bash
# Firmware (bench-test tanpa sensor/backend sungguhan)
cp include/secrets.h.example include/secrets.h
pio run -e esp32dev-bench --target upload
pio device monitor -b 115200

# Backend (di terminal lain)
cd backend
cp .env.example .env   # isi ADMIN_API_KEY
pip install -r requirements-dev.txt
uvicorn app.main:app --reload --port 8000
# buka http://localhost:8000 untuk dashboard
```

Detail lengkap: [[Firmware-Setup]] dan [[Backend-Setup]].
