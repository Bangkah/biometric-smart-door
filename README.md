# Biometric Smart Key & Management System
## Implementation Phase 2 — Local Event System & Memory Buffer

Firmware ESP32 untuk state machine kontrol akses biometrik (Phase 1)
ditambah sistem pencatatan log lokal persisten (Phase 2), sesuai SRS
Bab 20: **hanya perangkat edge**, tanpa backend/database/jaringan.

---

## 1. Struktur Proyek

```
biometric-smart-key/
├── platformio.ini
├── include/
│   └── config.h              # Pin mapping, timing, build mode, konfigurasi log
└── src/
    ├── main.cpp               # setup()/loop(), watchdog, debugger Serial
    ├── state_machine.h/.cpp   # State machine (SRS Bab 6) + trigger logEvent()
    ├── fingerprint.h/.cpp     # Abstraksi sensor sidik jari (mock + real)
    ├── actuator.h/.cpp        # Abstraksi relay/solenoid atau servo
    └── event_logger.h/.cpp    # [Phase 2] Ring buffer log persisten (LittleFS)
```

Kode ditulis untuk **PlatformIO** (disarankan) tetapi kompatibel dengan
Arduino IDE — lihat bagian 3.2.

---

## 2. Kebutuhan Perangkat Keras

| Komponen | Catatan |
|---|---|
| ESP32 DevKit (mis. ESP32-WROOM-32) | Wi-Fi tidak dipakai di Phase 1, tapi chip tetap ESP32 |
| Sensor sidik jari AS608 / R307 | **Opsional untuk Phase 1 awal** — lihat mode mock di bagian 5 |
| Relay module 1 channel + Solenoid Door Lock 12V, **atau** Servo (SG90) untuk prototipe meja | Pilih salah satu via `ACTUATOR_TYPE_SERVO` di `config.h` |
| 3x LED (Merah/Hijau/Biru) + resistor 220Ω, atau 1x LED RGB | Indikator status |
| Push button + resistor (atau pakai `INPUT_PULLUP` langsung ke GND) | Simulasi sentuhan jari saat sensor belum terpasang |
| Power supply terpisah untuk aktuator (12V untuk solenoid) | Jangan menyuplai solenoid dari 5V ESP32 |

### 2.1 Pin Mapping (default, ubah di `include/config.h`)

| Fungsi | Pin ESP32 |
|---|---|
| Fingerprint RX (ESP32 RX2 ← Sensor TX) | GPIO16 |
| Fingerprint TX (ESP32 TX2 → Sensor RX) | GPIO17 |
| Relay / Solenoid | GPIO26 |
| Servo (jika dipakai) | GPIO27 |
| LED Merah | GPIO25 |
| LED Hijau | GPIO33 |
| LED Biru | GPIO32 |
| Push button simulasi jari | GPIO4 (ke GND, `INPUT_PULLUP`) |

---

## 3. Build & Upload

### 3.1 PlatformIO (disarankan)

```bash
# Install PlatformIO CLI jika belum ada
pip install platformio

# Dari dalam folder biometric-smart-key/
pio run                 # compile
pio run --target upload # flash ke ESP32
pio device monitor -b 115200   # buka Serial Monitor
```

### 3.2 Arduino IDE (alternatif)

1. Buat sketch baru bernama `biometric-smart-key`, lalu ganti file `.ino`-nya
   dengan isi `src/main.cpp` (rename jadi `biometric-smart-key.ino`).
2. Salin `state_machine.h/.cpp`, `fingerprint.h/.cpp`, `actuator.h/.cpp`,
   dan `config.h` ke folder sketch yang sama (Arduino IDE otomatis
   meng-compile semua `.h`/`.cpp` dalam satu folder).
3. Install board **ESP32** via Boards Manager (Espressif Systems).
4. Jika `MOCK_FINGERPRINT_MODE` dinonaktifkan, install library
   **Adafruit Fingerprint Sensor Library** via Library Manager.
5. Pilih board ESP32 Dev Module, port yang sesuai, lalu Upload.

---

## 4. Wiring Ringkas

```
ESP32                         AS608/R307 (opsional, Phase 1 awal boleh skip)
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
  (pin dikonfigurasi INPUT_PULLUP, tidak perlu resistor eksternal)
```

**PENTING (grounding, SRS 4.5):** GND ESP32, relay module, dan PSU 12V
untuk solenoid harus memiliki referensi ground yang sama (common ground)
apabila tidak menggunakan isolasi galvanis (opto-isolator pada relay).

---

## 5. Bench Testing TANPA Sensor Fisik (Mock Mode)

Ini memungkinkan pengujian **seluruh state machine** sebelum sensor
AS608/R307 tersedia secara fisik — sesuai permintaan "Simulation
Requirements" SRS Bab 18.

### 5.1 Aktifkan mode mock

Di `include/config.h`, pastikan baris berikut **aktif** (default sudah aktif):

```cpp
#define MOCK_FINGERPRINT_MODE 1
```

### 5.2 Dua cara simulasi

**A. Via push button** (paling mendekati perilaku sensor nyata):
- Tekan tombol di GPIO4 → firmware memperlakukannya sebagai
  "jari terdeteksi" (IDLE → VERIFYING), dan otomatis menghasilkan
  hasil **MATCH** (slot dummy #1) → lanjut ke UNLOCKED.
- Cocok untuk menguji alur normal: buka kunci → auto-lock 5 detik.

**B. Via Serial Monitor** (untuk menguji semua jalur, termasuk DENIED):
1. Buka Serial Monitor, baud rate **115200**, line ending **Newline**.
2. Ketik `t` lalu Enter → mensimulasikan "ada sentuhan jari"
   (state IDLE → VERIFYING).
3. Firmware akan menunggu (maks. `FP_VERIFY_TIMEOUT_MS` = 2 detik):
   - Ketik `m` lalu Enter → simulasi **MATCH** → state ke `UNLOCKED`.
   - Ketik `x` lalu Enter → simulasi **NO MATCH** → state ke `DENIED`.
   - Tidak mengetik apa pun dalam 2 detik → dianggap `NO_MATCH` (timeout).

### 5.3 Skenario pengujian yang disarankan

| # | Skenario | Langkah | Hasil yang diharapkan |
|---|---|---|---|
| 1 | Boot normal | Power on / reset | Log: `BOOT → INITIALIZE → HARDWARE_CHECK → LOCKED → IDLE`; LED biru menyala |
| 2 | Akses valid | Di IDLE, kirim `t` lalu `m` (atau tekan tombol) | `IDLE → VERIFYING → UNLOCKED`; LED hijau; log "Akses diberikan"; relay aktif |
| 3 | Auto-lock | Tunggu 5 detik setelah UNLOCKED | `UNLOCKED → LOCKED → IDLE` otomatis; LED kembali biru; relay nonaktif |
| 4 | Akses ditolak | Di IDLE, kirim `t` lalu `x` | `IDLE → VERIFYING → DENIED → IDLE`; LED merah sesaat, lalu biru lagi |
| 5 | Timeout verifikasi | Di IDLE, kirim `t`, lalu jangan kirim apa pun selama >2 detik | Otomatis dianggap `NO_MATCH` → `DENIED → IDLE` |
| 6 | Watchdog / anomali hang | Kapan saja, kirim `h` lalu Enter | Log "Simulasi HANG dipicu"; loop diblokir; setelah ±8 detik ESP32 **reset sendiri** (lihat log reboot & kembali ke BOOT) |
| 7 | Fail-safe saat ERROR_SAFE | (Perlu modifikasi sementara `healthCheck()` agar `return false`, atau lepas kabel sensor jika mode real) | State masuk `ERROR_SAFE`; LED merah berkedip; relay dipaksa LOCKED; setelah 3 detik retry `HARDWARE_CHECK` |

### 5.4 Menguji ERROR_SAFE tanpa mengubah kode

Cara termudah untuk memicu `ERROR_SAFE` secara mock: buka
`src/fingerprint.cpp`, pada blok `#if MOCK_FINGERPRINT_MODE`, ubah
sementara `healthCheck()` agar `return false;`, upload ulang, amati
firmware masuk `ERROR_SAFE` dan mencoba recovery setiap
`ERROR_RECOVERY_DELAY_MS` (3 detik). Kembalikan ke `return true;`
setelah pengujian selesai.

---

## 6. Transisi ke Sensor Fisik (AS608/R307)

Setelah sensor tiba dan sudah didaftarkan template-nya (memakai sketch
enrollment bawaan library Adafruit, di luar cakupan firmware ini):

1. Nonaktifkan mock mode di `config.h`:
   ```cpp
   // #define MOCK_FINGERPRINT_MODE 1   // <- comment baris ini
   ```
2. Pastikan library **Adafruit Fingerprint Sensor Library** terpasang
   (sudah tercantum di `platformio.ini`).
3. Wiring sesuai bagian 4 (GPIO16/17 ke sensor UART).
4. Build & upload ulang. Tidak ada perubahan lain yang diperlukan —
   `state_machine.cpp` tidak menyentuh detail sensor secara langsung,
   sehingga logika alur tetap identik antara mode mock dan mode nyata.

---

## 7. Watchdog Timer (SRS 5.5)

- Diinisialisasi di `main.cpp` dengan timeout `WATCHDOG_TIMEOUT_S = 8`
  detik (dapat diubah di `config.h`).
- `loop()` memanggil `esp_task_wdt_reset()` di setiap iterasi normal.
- Semua fungsi state machine bersifat **non-blocking** (berbasis
  `millis()`, bukan `delay()` panjang) agar watchdog tidak ter-trigger
  saat operasi normal.
- Kode menangani dua API watchdog Arduino-ESP32 core (v2.x dan v3.x)
  secara otomatis — lihat komentar di `main.cpp`.
- Uji manual: kirim `h` via Serial Monitor (lihat skenario #6 di atas).

---

## 8. Phase 2 — Local Event System & Memory Buffer

### 8.1 Cara Kerja

- Setiap event disimpan sebagai `EventRecord` berukuran **tetap** (41 byte)
  ke dalam file ring buffer `EVENT_LOG_FILE` (`/events.dat`) di LittleFS,
  berkapasitas `EVENT_LOG_MAX_ENTRIES` (default **50**, ubah di `config.h`
  — bisa dinaikkan ke 100 sesuai kebutuhan).
- Saat buffer penuh, entri **terlama otomatis tertimpa** (perilaku ring
  buffer klasik) — sesuai permintaan SRS 5.6.
- Setiap `logEvent()` langsung **ditulis + di-flush ke flash** (bukan
  hanya disangga di RAM), sehingga log tidak hilang saat listrik padam
  mendadak (SRS 13.2).
- **Self-healing terhadap power-loss:** saat boot, `recoverStateFromFlash()`
  memindai seluruh slot dan menentukan posisi tulis berikutnya dari
  nomor urut (`sequence`) tertinggi yang valid — tidak bergantung pada
  file metadata terpisah yang bisa basi/korup. Worst case bila listrik
  padam persis di tengah satu penulisan: hanya 1 entri terakhir yang
  berpotensi hilang, struktur ring buffer tetap konsisten.
- Timestamp memakai `millis()` **relatif terhadap sesi boot saat ini**
  (belum ada RTC/NTP di Phase 2 — itu ranah backend/Phase 3+). Field
  `bootId` (tersimpan di NVS/Preferences, increment tiap boot)
  membedakan log dari sesi boot yang berbeda agar timestamp tidak
  disalahartikan sebagai waktu absolut yang sama.

### 8.2 Tipe Event

| EventType | Dipicu saat |
|---|---|
| `SYSTEM_BOOT` | Setiap kali firmware boot (state `BOOT`) |
| `HARDWARE_CHECK_FAIL` | `HARDWARE_CHECK` gagal (sensor/aktuator tidak OK) |
| `ACCESS_GRANTED` | Transisi ke `UNLOCKED` (sidik jari cocok) |
| `ACCESS_DENIED` | Transisi ke `DENIED` (sidik jari tidak dikenali) |
| `ERROR_SAFE_TRIGGERED` | Masuk ke state `ERROR_SAFE` |
| `RECOVERY_ATTEMPT` | `ERROR_SAFE` mencoba kembali ke `HARDWARE_CHECK` |
| `LOG_CLEARED` | Log dihapus manual via perintah debugger `c` |

### 8.3 Perintah Debugger Serial (tambahan Phase 2)

Selain `h` (uji watchdog, Phase 1), `t`/`m`/`x` (simulasi sidik jari):

| Perintah | Fungsi |
|---|---|
| `l` + Enter | Menampilkan seluruh log tersimpan (urut terlama → terbaru) ke Serial Monitor |
| `c` + Enter | Menghapus seluruh log (format ulang ring buffer), lalu mencatat satu event `LOG_CLEARED` |

### 8.4 Skenario Pengujian Phase 2

| # | Skenario | Langkah | Hasil yang diharapkan |
|---|---|---|---|
| 1 | Log boot tercatat | Power on / reset | Log real-time: `[EVENT #1] ... type=SYSTEM_BOOT`; ketik `l` → muncul di daftar |
| 2 | Log akses granted | Di IDLE, kirim `t` lalu `m` | Setelah `UNLOCKED`, muncul `[EVENT #N] ... type=ACCESS_GRANTED` |
| 3 | Log akses denied | Di IDLE, kirim `t` lalu `x` | Muncul `[EVENT #N] ... type=ACCESS_DENIED` |
| 4 | Log error safe | Picu `ERROR_SAFE` (lihat bagian 5.4) | Muncul `ERROR_SAFE_TRIGGERED`, lalu `RECOVERY_ATTEMPT` setelah 3 detik |
| 5 | **Ketahanan power-loss** | Lakukan beberapa akses (granted/denied), lalu cabut catu daya ESP32 **secara paksa** (jangan reset normal), nyalakan kembali | Ketik `l` setelah boot ulang → semua log sebelum pemadaman **masih ada**; `bootId` bertambah 1 menandai sesi baru |
| 6 | Ring buffer wrap-around | Set sementara `EVENT_LOG_MAX_ENTRIES` ke angka kecil (mis. 5) di `config.h`, upload ulang, lakukan >5 kali akses | Ketik `l` → hanya 5 log **terbaru** yang tersisa, entri tertua otomatis hilang |
| 7 | Hapus log | Ketik `c` lalu Enter | Log: "Log berhasil dihapus"; ketik `l` → hanya tersisa 1 entri `LOG_CLEARED` |

### 8.5 Catatan Partisi Flash

`platformio.ini` sudah diset `board_build.filesystem = littlefs` agar
partisi data diformat sebagai LittleFS. Jika memakai **Arduino IDE**,
pastikan memilih **Partition Scheme** yang menyediakan partisi
SPIFFS/data (mis. "Default 4MB with spiffs") di menu Tools — LittleFS
tetap dapat memakai partisi tersebut karena kode memanggil
`LittleFS.begin(true)` (auto-format sesuai filesystem yang diminta).

Ukuran ring buffer sangat kecil (41 byte × 50 entri ≈ 2 KB), sehingga
konsumsi flash dan dampak wear-leveling dapat diabaikan untuk
kebutuhan prototipe bangku.

---

## 9. Batasan Phase 1 & Phase 2 (sesuai cakupan SRS)

Fase ini **sengaja tidak mencakup** (baru masuk Phase 3+ sesuai SRS Bab 20):
- Komunikasi backend/API, autentikasi perangkat (Phase 3).
- Sinkronisasi/upload event log yang tersimpan ke server saat jaringan
  pulih — Phase 2 baru menyimpan & menampilkan log **secara lokal**;
  pengiriman ke backend adalah tanggung jawab Phase 3 (Backend &
  Device Communication).
- Web Dashboard, audit log terpusat (Phase 4).
- Remote unlock, command queue, enrollment jarak jauh (Phase 5/6).

Fase ini **memenuhi** dari SRS:
- FR-001 (verifikasi < 2 detik, via `FP_VERIFY_TIMEOUT_MS`)
- FR-003 (penolakan sidik jari tak dikenal)
- FR-004, FR-005 (unlock saat match, auto-lock setelah timeout)
- FR-006 (operasi mandiri tanpa jaringan)
- FR-007 (struktur data event lokal setiap upaya akses — `EventRecord`)
- FR-008 (penyanggaan event pada persistent storage lokal — ring buffer LittleFS)
- State machine & fail-safe sesuai SRS Bab 6 dan 13.3–13.5
- Watchdog sesuai SRS 5.5
- Local Events & Persistent Buffer sesuai SRS 5.6 dan 13.2 (Network Failure:
  log tetap aman tersimpan meski belum ada koneksi/backend untuk dikirim)

> **Catatan integrasi Phase 3:** saat backend siap, FR-009 (sinkronisasi
> event ke backend) dapat diimplementasikan dengan menambahkan fungsi
> `syncPendingEvents()` di `event_logger.cpp` yang membaca entri berurutan
> berdasarkan `sequence`, mengirimkannya ke endpoint `10.2 Events`, dan
> menandai batas `sequence` terakhir yang berhasil disinkronkan (disimpan
> terpisah di NVS) — tanpa perlu mengubah struktur ring buffer yang ada.
