<<<<<<< HEAD
# Biometric Smart Key & Management System
## Implementation Phase 1 — Hardware Prototype
## Implementation Phase 2 — Local Event System & Memory Buffer
## Implementation Phase 3 — Network, NTP Time Sync & Backend Event Upload

Firmware ESP32: state machine kontrol akses biometrik (Phase 1), log lokal
persisten (Phase 2), dan Wi-Fi + NTP + upload event ke backend (Phase 3),
sesuai SRS Bab 20. Edge Autonomy tetap dijaga (SRS 2.2/NFR-002): perangkat
berfungsi penuh secara lokal walau Wi-Fi/backend mati total.

---

## 1. Struktur Proyek

```
biometric-smart-key/
├── platformio.ini
├── include/
│   ├── config.h               # Pin mapping, timing, build mode, semua konfigurasi
│   └── secrets.h.example      # Template kredensial (salin -> secrets.h, JANGAN commit)
├── src/
│   ├── main.cpp                # setup()/loop() + network task, watchdog, debugger Serial
│   ├── state_machine.h/.cpp    # State machine (SRS Bab 6) + trigger logEvent()
│   ├── fingerprint.h/.cpp      # Abstraksi sensor sidik jari (mock + real)
│   ├── actuator.h/.cpp         # Abstraksi relay/solenoid atau servo
│   ├── event_logger.h/.cpp     # [Phase 2] Ring buffer log persisten (LittleFS) + epoch [Phase 3]
│   ├── network_manager.h/.cpp  # [Phase 3] Wi-Fi non-blocking + auto-reconnect
│   ├── time_sync.h/.cpp        # [Phase 3] Klien NTP (SNTP async bawaan ESP32)
│   ├── api_client.h/.cpp       # [Phase 3] Upload event ke backend (HTTP POST batch)
│   ├── backoff.h                # [Phase 3] Exponential backoff murni (dipakai 2 modul + test)
│   └── time_estimate.h          # [Phase 3] Estimasi epoch UTC murni (dipakai api_client + test)
├── test/                        # [Phase 3] Unit test native (host, tanpa ESP32) — lihat bagian 11
│   ├── common_stubs.cpp         # Implementasi stub Arduino/LittleFS/Preferences/FreeRTOS
│   ├── stubs/                   # Header stub (menimpa Arduino.h dkk via -I saat env:native)
│   ├── test_backoff/            # Unit test backoff.h
│   ├── test_time_estimate/      # Unit test time_estimate.h
│   └── test_event_logger/       # Unit test event_logger.cpp PRODUKSI (ring buffer, power-loss, epoch)
└── tools/
    └── mock_backend.py          # [Phase 3] Backend palsu (stdlib only) untuk bench-test lokal
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

## 9. Phase 3 — Konfigurasi Kredensial & Keamanan Transport

### 9.1 Menyiapkan `secrets.h`

```bash
cp include/secrets.h.example include/secrets.h
```

Isi `include/secrets.h` (file ini ada di `.gitignore`, **tidak pernah** ter-commit):

```cpp
#define WIFI_SSID           "NamaWiFiAnda"
#define WIFI_PASSWORD       "PasswordWiFiAnda"
#define DEVICE_ID           "DOOR-01"
#define DEVICE_API_TOKEN    "token-unik-dari-backend"
#define BACKEND_EVENTS_ENDPOINT "https://backend-anda.example.com/api/v1/events"
```

Tanpa `secrets.h` sama sekali, firmware **tetap compile dan tetap berjalan** —
`WIFI_SSID` kosong membuat `NetworkManager` masuk state `NotConfigured` dan
perangkat berperilaku identik dengan Phase 1/2 (murni offline). Ini yang
membuat CI (`pio run` tanpa `secrets.h`) tidak pernah gagal hanya karena
kredensial tidak ada.

### 9.2 Kebijakan TLS (SRS NFR-001: wajib terenkripsi)

`ApiClient::begin()` bersifat **fail-closed** — upload dimatikan (bukan
di-bypass diam-diam) jika kebijakan keamanan tidak terpenuhi:

| Kondisi | Perilaku |
|---|---|
| `BACKEND_EVENTS_ENDPOINT` diawali `http://`, `BACKEND_ALLOW_PLAIN_HTTP=0` (default) | **Upload dinonaktifkan total.** Build normal (`esp32dev`) SELALU menolak HTTP polos. |
| `http://` + `BACKEND_ALLOW_PLAIN_HTTP=1` (hanya env `esp32dev-bench`) | Diizinkan, dengan peringatan di Serial. Untuk `tools/mock_backend.py` di LAN saja. |
| `https://` tanpa `BACKEND_ROOT_CA` dan tanpa `BACKEND_TLS_INSECURE=1` | **Upload dinonaktifkan.** Isi root CA di `secrets.h` atau set flag insecure untuk dev. |
| `https://` + `BACKEND_ROOT_CA` terisi | Jalur produksi: sertifikat backend diverifikasi penuh. |
| `https://` + `BACKEND_TLS_INSECURE=1`, root CA kosong | Diizinkan untuk dev, TLS tanpa verifikasi sertifikat — peringatan di Serial tiap boot. |

Perangkat yang gagal validasi ini **tidak crash dan tidak berhenti bekerja**
secara lokal (SRS 13.6 Backend Failure) — hanya modul upload yang idle;
pintu, sidik jari, dan log lokal tetap berfungsi penuh.

---

## 10. Phase 3 — Wi-Fi, NTP, dan Upload Event

### 10.1 Arsitektur: Network Task Terpisah

```
core 1 (Arduino loop default): watchdog kick -> debugger Serial -> state machine
core 0 (task baru "net_task") : Wi-Fi reconnect -> NTP polling -> upload backend
```

`state_machine.cpp` **tidak pernah** memanggil apa pun dari `network_manager`,
`time_sync`, atau `api_client` secara langsung — satu-satunya titik hubung
adalah `eventLogger.setTimeProvider(...)` (dependency injection, bukan
pemanggilan langsung). Ini memastikan Wi-Fi lambat/mati **tidak pernah**
memperlambat verifikasi sidik jari atau menunda watchdog — Edge Autonomy
(SRS 2.2) dan NFR-002 tetap terjaga persis seperti Phase 1/2.

`net_task` **sengaja tidak didaftarkan** ke hardware/software watchdog:
jaringan yang macet total tidak boleh sampai me-reset (dan berpotensi
mengganggu) mekanisme kunci pintu. Setiap panggilan jaringan (koneksi Wi-Fi,
NTP, HTTP) dibatasi timeout sendiri sebagai gantinya.

### 10.2 Wi-Fi Connection Manager

- State machine kecil: `NotConfigured → Connecting → Connected` dengan
  `Disconnected`/`Reconnecting` untuk kegagalan, mirip pola Phase 1.
- **Exponential backoff** (`backoff.h`, dipakai bersama `ApiClient`):
  percobaan ke-N menunggu `min(WIFI_BACKOFF_BASE_MS × 2^N, WIFI_BACKOFF_MAX_MS)`
  detik — default 2 detik → 4 → 8 → ... maksimum 60 detik, supaya tidak
  membanjiri access point saat sinyal hilang lama.
- Alasan disconnect terakhir (password salah, AP tidak ditemukan, dst)
  ditangkap dari event Wi-Fi dan ditampilkan di perintah `w` untuk
  mempermudah diagnosis di lapangan.

### 10.3 NTP Time Sync

- Memakai SNTP bawaan ESP32 (`configTime()`), berjalan asinkron —
  `TimeSync::update()` hanya **polling** `time(nullptr)`, tidak pernah
  blocking menunggu respons NTP.
- Epoch yang disimpan (`EventRecord::unixTimestamp`) **selalu UTC**;
  `NTP_GMT_OFFSET_SEC` (default WIB/UTC+7) hanya memengaruhi tampilan
  `getFormattedTime()` di perintah `w`, bukan nilai yang disimpan/dikirim.
- Waktu sistem **hilang saat reboot** (ESP32 tanpa RTC baterai). Event yang
  tercatat sebelum NTP sinkron kembali diberi `unixTimestamp=0` saat
  dicatat; `ApiClient` mengisi perkiraannya saat upload (lihat 10.4).

### 10.4 Event Sync / Backend Upload

- `ApiClient` melacak progres sinkronisasi sebagai pasangan
  **(`log_epoch`, `lastSyncedSequence`)** di NVS — bukan hanya sequence
  saja. `log_epoch` naik setiap `eventLogger.clearAll()` dipanggil (perintah
  `c`), sehingga event baru pasca-hapus log (yang sequence-nya mulai dari 1
  lagi) **tidak pernah** dianggap "sudah tersinkron" secara keliru.
- **Kebijakan pengiriman: at-least-once, tanpa kehilangan.** Penanda hanya
  maju setelah backend membalas `2xx` (atau `409` = sudah pernah diterima).
  Kegagalan apa pun (timeout, 5xx, token ditolak) membuat batch yang sama
  dicoba lagi dengan backoff eksponensial (`SYNC_RETRY_BASE_MS` s/d
  `SYNC_RETRY_MAX_MS`) — backend **wajib idempoten** berdasarkan kunci unik
  `(device_id, log_epoch, boot_id, sequence)`.
- Event yang keburu **tertimpa ring buffer** sebelum sempat terkirim (offline
  sangat lama) terdeteksi otomatis; jumlahnya dilaporkan lewat field
  `missed_events` di payload — bukan disembunyikan diam-diam.
- Payload JSON per event menyertakan `time_source`: `"ntp"` (tercatat saat
  NTP sudah sinkron), `"estimated"` (diperkirakan dari uptime sesi berjalan,
  lihat `time_estimate.h`), atau `"unsynced"` (sesi lama, waktu riil tidak
  dapat dipulihkan) — backend dapat memutuskan tingkat kepercayaan waktunya.

### 10.5 Perintah Debugger Baru

| Perintah | Fungsi |
|---|---|
| `w` + Enter | Status Wi-Fi (state, IP, RSSI), NTP (waktu terformat), dan sync (event menunggu, hasil terakhir, retry berikutnya) |
| `s` + Enter | Minta sinkronisasi ke backend **segera** (tetap dieksekusi di `net_task`, bukan blocking `loop()`) |

---

## 11. Bench Testing Phase 3 Tanpa Backend Sungguhan

### 11.1 Jalankan mock backend

```bash
python tools/mock_backend.py --token dev-token-123 --port 8000
```

Script ini murni stdlib Python (tanpa dependensi eksternal), mendukung:
validasi Bearer token, **deduplikasi idempoten** berbasis
`(device_id, log_epoch, boot_id, sequence)` (membuktikan retry firmware
tidak membuat data ganda di backend), dan flag `--fail-next N` untuk
mensimulasikan backend down N request pertama (menguji backoff retry).

### 11.2 Konfigurasi `secrets.h` untuk bench

```cpp
#define DEVICE_API_TOKEN        "dev-token-123"
#define BACKEND_EVENTS_ENDPOINT "http://<IP-LAN-PC-Anda>:8000/api/v1/events"
```

### 11.3 Build & flash dengan env bench

```bash
pio run -e esp32dev-bench --target upload
pio device monitor -b 115200
```

`esp32dev-bench` mendefinisikan `BACKEND_ALLOW_PLAIN_HTTP=1` sehingga
endpoint `http://` di atas diterima — env `esp32dev` (produksi) akan
menolaknya sesuai kebijakan di bagian 9.2.

### 11.4 Skenario Uji

| # | Skenario | Langkah | Hasil yang diharapkan |
|---|---|---|---|
| 1 | Koneksi Wi-Fi | Boot dengan `secrets.h` terisi benar | Log `[NETWORK] Terhubung! IP: ...`; ketik `w` → state `CONNECTED` |
| 2 | Auto-sync berkala | Lakukan beberapa akses (lihat skenario Phase 1/2), tunggu `SYNC_INTERVAL_MS` | Terminal mock backend menampilkan event `NEW` sesuai urutan |
| 3 | Sync manual | Ketik `s` | `[API_CLIENT] Sukses (HTTP 201)...`; mock backend mencetak event yang sama |
| 4 | Idempotensi | Ketik `s` dua kali berturut-turut tanpa event baru | Kedua kali `NOTHING_TO_SYNC` (bukan mengirim ulang) |
| 5 | Wi-Fi putus | Matikan AP / jauhkan device, amati Serial | `[NETWORK] Koneksi Wi-Fi terputus`, retry dengan backoff; akses sidik jari **tetap berfungsi normal** |
| 6 | Backend down + retry | Jalankan mock backend dengan `--fail-next 3`, ketik `s` | 3× `HTTP_ERROR` (503) dengan jeda backoff membesar, percobaan ke-4 `SUCCESS` |
| 7 | Offline lama (event tertimpa) | Matikan Wi-Fi, lakukan >50 akses (lebihi `EVENT_LOG_MAX_ENTRIES`), nyalakan lagi | Payload sync berikutnya punya `missed_events > 0`, dilaporkan di Serial |
| 8 | Reboot saat pending sync | Lakukan akses, cabut daya SEBELUM sync berikutnya, nyalakan lagi | Setelah Wi-Fi tersambung kembali, event lama (sequence lama) tetap terkirim — penanda `lastSyncedSequence` bertahan di NVS |
| 9 | Hapus log lalu event baru | Ketik `c`, lakukan akses baru, ketik `s` | Event baru (epoch baru) terkirim; **tidak** dianggap "sudah tersinkron" walau sequence-nya kecil |
| 10 | Token ditolak | Ubah `DEVICE_API_TOKEN` di firmware agar tidak cocok dengan `--token` mock backend | `[API_CLIENT] GAGAL (HTTP 401)`; event tidak dibuang, retry terus (periksa token) |

---

## 12. Unit Testing (Phase 3 Quality Gate)

### 12.1 Cakupan

Logika yang **dapat diuji murni** (tanpa ESP32/Wi-Fi/hardware) diekstrak ke
header/cpp terpisah, lalu diuji nyata di host lewat PlatformIO native test:

| Suite | Menguji | Yang diverifikasi |
|---|---|---|
| `test_backoff` | `backoff.h` (`computeBackoffDelay`) | Percobaan pertama = base delay, delay dobel tiap percobaan, dibatasi max, tidak overflow walau percobaan sangat besar |
| `test_time_estimate` | `time_estimate.h` (`estimateUnixForUptime`) | 0 saat belum sync, estimasi benar untuk event lampau, aman terhadap millis() rollover (~49 hari uptime) |
| `test_event_logger` | `event_logger.cpp` **PRODUKSI SESUNGGUHNYA** (bukan tiruan) | Ring buffer wrap-around, **ketahanan power-loss** (reboot disimulasikan dengan instance `EventLogger` baru di atas "flash" yang sama), epoch naik saat `clearAll()`, epoch persisten lintas reboot, injeksi `unixTimestamp` via `setTimeProvider()` |

`test_event_logger` adalah yang paling penting: ia mengompilasi
`src/event_logger.cpp` yang SAMA PERSIS dipakai firmware, dipasangkan dengan
LittleFS/Preferences/FreeRTOS **palsu tapi fungsional** (in-memory, lihat
`test/stubs/`) yang tetap "hidup" antar-instance `EventLogger` dalam satu
proses — inilah yang memungkinkan mensimulasikan reboot ESP32 (power-loss)
tanpa mem-flash hardware sungguhan.

`network_manager.cpp`, `time_sync.cpp`, dan `api_client.cpp` **tidak**
dikompilasi di `env:native` karena men-`#include` header Wi-Fi/HTTP/TLS asli
yang tidak tersedia di host — bagian logikanya yang bisa diuji murni sudah
diekstrak ke `backoff.h`/`time_estimate.h`, sisanya divalidasi lewat build
`esp32dev` (compile check) + bench-test manual di bagian 11.

### 12.2 Menjalankan

```bash
pio test -e native
```

Perintah ini menjalankan **ketiga** suite sekaligus dan melaporkan hasilnya
per test case. CI (bagian 13) menjalankan perintah yang sama sebagai gate
wajib SEBELUM build ESP32 dijalankan (fail-fast: kesalahan logika ketahuan
dalam hitungan detik, bukan menunggu toolchain ESP32 selesai di-download).

### 12.3 Menambah Test Baru

1. Logika baru yang ingin diuji murni → ekstrak ke header/`.cpp` tanpa
   `#include <Arduino.h>`/`<WiFi.h>`/dst (pola `backoff.h`/`time_estimate.h`),
   ATAU jika perlu menguji `.cpp` yang sudah memakai `Arduino.h`/`LittleFS.h`/
   `Preferences.h` (pola `event_logger.cpp`), pastikan stub fungsional yang
   relevan sudah ada di `test/stubs/`.
2. Buat folder baru `test/test_<nama>/` berisi satu file `test_<nama>.cpp`
   dengan `setUp()`/`tearDown()` + `RUN_TEST(...)` per skenario (contoh:
   `test/test_event_logger/test_event_logger.cpp`).
3. Jika suite baru butuh file `.cpp` tambahan dari `src/`, tambahkan ke
   daftar `+<...>` di `build_src_filter` pada `[env:native]` (`platformio.ini`).

---

## 13. CI/CD — Quality Gate GitHub Actions

`.github/workflows/firmware-ci.yml` menjalankan dua lapis gate berurutan
pada setiap Pull Request ke `main` dan setiap push ke branch `phase-*`:

1. **`unit-tests`** — `pio test -e native` (bagian 12). Jalan duluan karena
   cepat (tanpa toolchain ESP32): kesalahan logika ketahuan dalam hitungan
   detik.
2. **`build`** — hanya jalan jika `unit-tests` lulus (`needs: unit-tests`).
   Matrix dua environment: `esp32dev` (produksi, TLS wajib) dan
   `esp32dev-bench` (HTTP lokal diizinkan, untuk validasi kompatibilitas
   `tools/mock_backend.py`). Diikuti `pio check` (laporan, belum
   menggagalkan pipeline) dan ringkasan ukuran firmware.

PR **tidak dianggap lulus quality gate** sebelum kedua job ini hijau.
`platform = espressif32 @ ^6.9.0` di `platformio.ini` dipin persis (bukan
`latest`) supaya hasil build CI reproducible dari waktu ke waktu.

---

## 14. Batasan Phase 1–3 (sesuai cakupan SRS)

Fase ini **sengaja tidak mencakup** (masuk Phase 4+ sesuai SRS Bab 20):
- Web Dashboard, audit log terpusat lintas-perangkat (Phase 4).
- Remote unlock, command queue, enrollment jarak jauh (Phase 5/6).
- Endpoint `10.1 Device Authentication` dinamis — Phase 3 masih memakai
  `DEVICE_API_TOKEN` statis per perangkat; token dinamis via endpoint
  otentikasi adalah kandidat penguatan Phase 5/6 (SRS 12.4 Token Security).

Fase ini **memenuhi** dari SRS:
- FR-001 (verifikasi < 2 detik) · FR-003 (tolak sidik jari tak dikenal) ·
  FR-004/FR-005 (unlock + auto-lock) · FR-006 (mandiri tanpa jaringan)
- FR-007/FR-008 (struktur event lokal + persistent buffer, Phase 2)
- **FR-009** (perangkat mengirim event tersimpan ke backend saat jaringan
  terhubung — `ApiClient`, Phase 3)
- **FR-015** (heartbeat periodik — direpresentasikan lewat `device_heartbeat`
  implisit melalui event sync berkala; heartbeat eksplisit terpisah adalah
  kandidat penguatan Phase 4 saat endpoint `10.3 Device Heartbeat` backend
  tersedia)
- State machine & fail-safe (SRS Bab 6, 13.3–13.5) · Watchdog (SRS 5.5) ·
  Local Events & Persistent Buffer (SRS 5.6, 13.2)
- **NFR-001** (seluruh komunikasi wajib TLS — divalidasi & fail-closed di
  `ApiClient::begin()`, bagian 9.2)
- **NFR-002** (verifikasi lokal tidak bergantung jaringan — dijamin
  arsitektur network task terpisah, bagian 10.1)
- **SRS 12.3/12.4** (identitas & token perangkat — `DEVICE_ID`/`DEVICE_API_TOKEN`,
  dikirim via header `Authorization`/`X-Device-Id`, tidak pernah dicetak ke Serial)
- **SRS 12.5 Replay Protection**-nya versi Phase 3: idempotensi berbasis
  kunci `(device_id, log_epoch, boot_id, sequence)` di sisi backend
  (dicontohkan di `tools/mock_backend.py`)
- **SRS 13.6 Backend Failure**: kegagalan backend/jaringan total tidak
  mengganggu fungsi buka-kunci lokal (arsitektur network task + fail-closed
  `ApiClient`)

> **Catatan integrasi Phase 4 (Web Dashboard):** backend yang menerima
> payload `POST /api/v1/events` dari `ApiClient` (bagian 10.4) sudah
> memiliki semua field yang dibutuhkan dashboard audit log (SRS 11.6):
> `device_id`, `sequence`, `boot_id`, `timestamp`/`time_source`, `type`,
> `detail`, `message`. Tidak ada perubahan skema event yang diperlukan
> saat Phase 4 dimulai — hanya backend & dashboard baru yang perlu dibangun.
=======
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
>>>>>>> 1cc329cbad3eb075e126de42a0c9c30cd478a957
