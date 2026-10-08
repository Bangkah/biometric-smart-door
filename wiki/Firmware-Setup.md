# Firmware Setup

Panduan lengkap: hardware, wiring, build, dan bench-test firmware ESP32
(Phase 1-4) tanpa memerlukan sensor fisik maupun backend sungguhan lebih
dulu.

## Kebutuhan Perangkat Keras

| Komponen | Catatan |
|---|---|
| ESP32 DevKit (mis. ESP32-WROOM-32) | |
| Sensor sidik jari AS608 / R307 | **Opsional** untuk pengembangan awal — lihat Mode Mock di bawah |
| Relay module 1 channel + Solenoid Door Lock 12V, **atau** Servo (SG90) | Pilih via `ACTUATOR_TYPE_SERVO` di `config.h` |
| 3× LED (Merah/Hijau/Biru) + resistor 220Ω, atau 1× LED RGB | Indikator status |
| Push button | Simulasi sentuhan jari saat sensor belum terpasang |
| Power supply terpisah untuk aktuator (12V solenoid) | Jangan suplai solenoid dari 5V ESP32 |

### Pin Mapping (default, ubah di `include/config.h`)

| Fungsi | Pin ESP32 |
|---|---|
| Fingerprint RX (ESP32 RX2 ← Sensor TX) | GPIO16 |
| Fingerprint TX (ESP32 TX2 → Sensor RX) | GPIO17 |
| Relay / Solenoid | GPIO26 |
| Servo (jika dipakai) | GPIO27 |
| LED Merah / Hijau / Biru | GPIO25 / 33 / 32 |
| Push button simulasi jari | GPIO4 (ke GND, `INPUT_PULLUP`) |

### Wiring

```
ESP32                         AS608/R307 (opsional)
  GPIO16 (RX2) ------------------ TX
  GPIO17 (TX2) ------------------ RX
  5V/3V3 --------------------- VCC (cek datasheet modul)
  GND ------------------------ GND

ESP32                         Relay Module -> Solenoid 12V
  GPIO26 --------------------- IN
  5V -------------------------- VCC (relay)
  GND ------------------------- GND
                                Solenoid disuplai dari PSU 12V terpisah,
                                melalui kontak NO/COM relay.

ESP32                         LED Indikator
  GPIO25 --[220Ω]------------- Anoda LED Merah -> Katoda -> GND
  GPIO33 --[220Ω]------------- Anoda LED Hijau -> Katoda -> GND
  GPIO32 --[220Ω]------------- Anoda LED Biru  -> Katoda -> GND

ESP32                         Push Button (simulasi sentuhan jari)
  GPIO4  ---------------------- salah satu kaki tombol
  GND    ---------------------- kaki tombol lainnya
```

**PENTING (grounding, SRS 4.5):** GND ESP32, relay module, dan PSU 12V
untuk solenoid harus memiliki referensi ground yang sama apabila tidak
menggunakan isolasi galvanis (opto-isolator pada relay).

## Build & Upload

### PlatformIO (disarankan)

```bash
pip install platformio
pio run                       # compile (env default: esp32dev)
pio run --target upload       # flash ke ESP32
pio device monitor -b 115200  # Serial Monitor
```

Environment yang tersedia (`platformio.ini`):

| Environment | Kegunaan |
|---|---|
| `esp32dev` | Build produksi: Phase 4 aktif, TLS wajib untuk backend |
| `esp32dev-bench` | Sama seperti di atas, tapi mengizinkan `http://` untuk `tools/mock_backend.py` di LAN |
| `esp32dev-phase3only` | `ENABLE_PHASE4_FEATURES=0` dan folder `src/phase4/` dikeluarkan dari build — pembuktian batas fase, lihat [[Architecture]] |
| `native` | Unit test di host (tanpa ESP32), lihat [[Testing]] |

### Arduino IDE (alternatif)

1. Buat sketch baru, ganti file `.ino` dengan isi `src/main.cpp`.
2. Salin seluruh isi `src/` (termasuk subfolder `phase4/`) dan
   `include/config.h` ke folder sketch yang sama.
3. Install board **ESP32** via Boards Manager, dan library
   **Adafruit Fingerprint Sensor Library** + **ArduinoJson** via Library
   Manager.
4. Pilih board ESP32 Dev Module, port yang sesuai, lalu Upload.

## Konfigurasi Kredensial (`secrets.h`)

```bash
cp include/secrets.h.example include/secrets.h
```

```cpp
#define WIFI_SSID                "NamaWiFiAnda"
#define WIFI_PASSWORD            "PasswordWiFiAnda"
#define DEVICE_ID                "DOOR-01"
#define DEVICE_API_TOKEN         "token-dari-backend"     // POST /api/v1/devices
#define DEVICE_HMAC_SECRET       "hmac-secret-dari-backend"  // [Phase 4] idem
#define BACKEND_EVENTS_ENDPOINT  "https://backend-anda.example.com/api/v1/events"
```

`secrets.h` ada di `.gitignore` — **tidak pernah** ter-commit. Tanpa file
ini sama sekali, firmware tetap compile dan berjalan **offline-only**
(`WIFI_SSID` kosong → `NetworkManager` idle permanen). Lihat
[[Security]] untuk kebijakan TLS lengkap.

## Mode Mock (Tanpa Sensor Fisik)

Memungkinkan pengujian **seluruh state machine** tanpa sensor AS608/R307
(SRS Bab 18, Simulation Requirements). Aktif secara default:

```cpp
#define MOCK_FINGERPRINT_MODE 1   // include/config.h
```

Dua cara simulasi:

- **Push button** (GPIO4): tekan → selalu `MATCH` slot #1, lanjut `UNLOCKED`.
- **Serial Monitor** (115200 baud, line ending Newline): ketik `t` lalu
  Enter untuk simulasi sentuhan, lalu `m` (match) atau `x` (no match)
  dalam 2 detik.

### Perintah Debugger Serial (semua fase)

| Perintah | Fase | Fungsi |
|---|---|---|
| `t` / `m` / `x` | 1 | Simulasi sentuhan jari / match / no-match (mode mock) |
| `h` | 1 | Simulasi hang untuk menguji watchdog |
| `l` | 2 | Tampilkan seluruh event log tersimpan |
| `c` | 2 | Hapus seluruh event log |
| `w` | 3 | Status Wi-Fi/NTP/sync/command jarak jauh |
| `s` | 3 | Minta sinkronisasi log ke backend sekarang |

## Skenario Pengujian Inti

| # | Skenario | Langkah | Hasil yang diharapkan |
|---|---|---|---|
| 1 | Boot normal | Power on / reset | `BOOT→INITIALIZE→HARDWARE_CHECK→LOCKED→IDLE`; LED biru |
| 2 | Akses valid | IDLE, kirim `t` lalu `m` | `UNLOCKED`; LED hijau; auto-lock 5 detik kemudian |
| 3 | Akses ditolak | IDLE, kirim `t` lalu `x` | `DENIED` sesaat, LED merah, kembali `IDLE` |
| 4 | Watchdog | Kirim `h` | Loop diblokir; ESP32 **reset sendiri** setelah ±8 detik |
| 5 | ERROR_SAFE | Ubah sementara `healthCheck()` sensor jadi `return false` | Masuk `ERROR_SAFE`, LED merah berkedip, retry tiap 3 detik |
| 6 | Power-loss log | Lakukan beberapa akses, cabut daya paksa, nyalakan lagi | `l` setelah boot → semua log sebelumnya **masih ada** |
| 7 | Wi-Fi putus | Matikan AP, amati Serial | Reconnect dengan backoff; **akses lokal tetap normal** |

Skenario lengkap untuk Wi-Fi/backend/command jarak jauh: lihat
[[Testing]] dan [[Remote-Commands]].

## Transisi ke Sensor Fisik

1. Nonaktifkan mock mode: comment `#define MOCK_FINGERPRINT_MODE 1` di `config.h`.
2. Wiring sesuai bagian atas (GPIO16/17 ke UART sensor).
3. Build & upload ulang — tidak ada perubahan kode lain yang diperlukan.

## Watchdog Timer (SRS 5.5)

- Timeout default 8 detik (`WATCHDOG_TIMEOUT_S`), `loop()` me-reset
  watchdog tiap iterasi normal.
- `net_task` **sengaja tidak terdaftar** ke watchdog (lihat [[Architecture]]).
- Kode menangani dua API watchdog Arduino-ESP32 core (v2.x dan v3.x)
  secara otomatis.
