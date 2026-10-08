# Troubleshooting

## Git

### `fatal: refusing to merge unrelated histories`

Terjadi bila folder lokal di-`git init` sendiri (root commit baru, tidak
berhubungan dengan histori `origin/main`). **Jangan** `git init` lalu
langsung push. Solusi: selalu mulai dari `git fetch origin` lalu
`git checkout -b <branch> origin/main` — lihat [[Contributing]].

## Firmware

### Build gagal: `fatal error: secrets.h: No such file or directory`

`config.h` memakai `#if __has_include("secrets.h")` — seharusnya tidak
pernah fatal error walau file tidak ada. Jika tetap muncul, pastikan
toolchain compiler mendukung `__has_include` (GCC 5+/Clang, bawaan
ESP32 Arduino core modern). Sebagai workaround cepat:
`cp include/secrets.h.example include/secrets.h`.

### Device tidak pernah `CONNECTED` (`w` selalu `RECONNECTING`)

- Periksa `WIFI_SSID`/`WIFI_PASSWORD` di `secrets.h`.
- Lihat kode alasan disconnect di output `w` (mis. `202 AUTH_FAIL` =
  password salah, `201 NO_AP_FOUND` = SSID tidak terjangkau/salah ketik).
- ESP32 hanya mendukung Wi-Fi 2.4 GHz — pastikan access point tidak
  band-5GHz-only.

### Upload event selalu `HTTP_ERROR`, backend tidak terima apa pun

- Pastikan `BACKEND_EVENTS_ENDPOINT` memakai `https://` di build
  `esp32dev`, atau `esp32dev-bench` jika memakai `http://` lokal.
- `w` menampilkan HTTP code terakhir — `401` = token salah/device belum
  terdaftar; `0`/negatif = gagal koneksi/DNS/TLS (cek IP & port).

### `COMMAND_REJECTED: *** SIGNATURE TIDAK VALID ***`

`DEVICE_HMAC_SECRET` di firmware tidak cocok dengan yang tersimpan di
backend untuk device tersebut. Daftarkan ulang device
(`POST /api/v1/devices`) atau minta admin generate ulang kredensial,
lalu update `secrets.h`.

### `REMOTE_UNLOCK` tidak pernah tereksekusi, akhirnya `expired`

Device kemungkinan tidak pernah mencapai state `IDLE` (mis. macet di
`ERROR_SAFE`, atau terus-menerus di `VERIFYING`/`UNLOCKED` karena ada
yang menahan tombol mock). Cek `w` untuk state Serial log terakhir.

### Watchdog reset berulang-ulang tanpa sebab jelas

Pastikan tidak ada `delay()` panjang (>`WATCHDOG_TIMEOUT_S` detik) di
jalur manapun dalam `loop()`. Semua state machine seharusnya
non-blocking (berbasis `millis()`). Jika menambah kode baru di
`loop()`/`state_machine.cpp`, hindari operasi blocking.

## Backend

### `RuntimeError: ADMIN_API_KEY belum diset`

Backend sengaja menolak start tanpa ini (fail-closed). Salin
`backend/.env.example` → `backend/.env`, isi `ADMIN_API_KEY` dengan nilai
acak.

### Test backend gagal dengan error terkait `Settings`/`get_settings`

Pastikan environment variable (`ADMIN_API_KEY`, `DATABASE_URL`,
`PHASE4_ENABLED`) di-set lewat `monkeypatch` di fixture, **bukan**
diandalkan dari `.env` file saat test (`conftest.py` sudah menangani ini
— lihat fixture `_build_client`).

### Perubahan `PHASE4_ENABLED` tidak terlihat efeknya

Pastikan kode memanggil `create_app(get_settings())` untuk membangun
instance baru, **bukan** mengimpor `app.main.app` (singleton top-level
yang hanya dibangun sekali saat modul pertama di-import). Lihat
[[Architecture]] bagian "Backend" untuk penjelasan lengkap pola factory
ini.

### `409 Conflict` saat mendaftarkan device/kredensial

Device `id` atau kombinasi `(device_id, sensor_slot_id)` sudah ada.
Gunakan `id`/slot lain, atau cabut kredensial lama lebih dulu
(`DELETE /api/v1/users/{id}/credentials/{cred_id}`).

## Mock Backend (`tools/mock_backend.py`)

### `--enqueue` gagal konek (`ConnectionRefusedError`)

Pastikan instance server utama (tanpa `--enqueue`) sudah berjalan lebih
dulu di terminal lain, dan `--admin-port` yang dipakai `--enqueue` cocok
dengan server (default sama-sama `8001`, tidak perlu diisi manual kecuali
diubah eksplisit).

### Firmware tidak pernah menerima command dari mock backend

Pastikan `--token` dan `--hmac-secret` yang dipakai menjalankan mock
backend **sama persis** dengan `DEVICE_API_TOKEN`/`DEVICE_HMAC_SECRET` di
`secrets.h` firmware, dan `BACKEND_EVENTS_ENDPOINT` mengarah ke IP LAN
komputer yang menjalankan mock backend (bukan `localhost`/`127.0.0.1`,
karena itu merujuk ke ESP32 itu sendiri).
