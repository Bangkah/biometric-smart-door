# Architecture

## Gambaran Umum

```
┌─────────────────────────── ESP32 (Edge Device) ───────────────────────────┐
│                                                                             │
│  core 1 — loop() Arduino                    core 0 — net_task (FreeRTOS)  │
│  ┌─────────────────────────┐                ┌──────────────────────────┐ │
│  │ watchdog kick             │                │ NetworkManager (Wi-Fi)   │ │
│  │ debugger Serial (h/l/c/w/s)│               │ TimeSync (NTP)           │ │
│  │ StateMachine.update()      │◄──┐  dependency│ ApiClient (upload+hb)   │ │
│  │  BOOT→INIT→HW_CHECK→LOCKED │   │  injection │ CommandClient [Phase 4] │ │
│  │  →IDLE→VERIFYING→UNLOCKED  │   │  (waktu)   │  (poll+verify+ack)      │ │
│  │  →DENIED/ERROR_SAFE        │   │            └──────────┬───────────────┘ │
│  └──────────┬──────────────────┘   │                       │ HTTPS           │
│             │ EventLogger (mutex,  │                       │                 │
│             │ ring buffer LittleFS)│                       │                 │
│             └───────────────────────┘                       │                 │
│  Hanya titik hubung loop<->net: eventLogger.setTimeProvider()+mutex,        │
│  CommandClient::consumeUnlockRequest()/reportUnlockResult() (bridge mutex). │
└──────────────────────────────────────────────────────────────┬────────────┘
                                                                  │ HTTPS (Bearer + HMAC)
                                                                  ▼
                                               ┌──────────────────────────────┐
                                               │ Backend FastAPI               │
                                               │  Phase 3: events, heartbeat,  │
                                               │           devices            │
                                               │  Phase 4: users, commands     │
                                               │           (toggle PHASE4_ENABLED)│
                                               │  SQLite/PostgreSQL             │
                                               │  Dashboard statis (vanilla JS) │
                                               └──────────────────────────────┘
```

## Prinsip Desain

### 1. Edge Autonomy (SRS 2.2 / NFR-002)

Kunci pintu **tidak pernah** bergantung pada jaringan. `loop()` (core 1)
yang memegang state machine dan watchdog **tidak pernah memanggil** apa
pun dari modul jaringan secara langsung. Satu-satunya jalur komunikasi
lintas-task:

- `EventLogger::setTimeProvider()` — dependency injection satu arah
  (net_task menyuntik fungsi pembaca waktu; loop task memanggilnya tanpa
  tahu detail NTP).
- `CommandClient::consumeUnlockRequest()` / `reportUnlockResult()`
  (Phase 4) — bridge bermutex, satu slot in-flight, dengan auto-expiry
  supaya permintaan yang tak terkonsumsi (mis. device macet di
  `ERROR_SAFE`) tidak pernah menggantung selamanya.

`net_task` **sengaja tidak didaftarkan ke watchdog**: jaringan yang total
macet tidak boleh berakibat ESP32 reset (dan berpotensi mengganggu siklus
kunci). Setiap panggilan HTTP dibatasi timeout sendiri sebagai gantinya.

### 2. Fail-Closed, bukan Fail-Open

Di setiap titik keputusan keamanan, kegagalan mengarah ke **penolakan**,
bukan ke jalan pintas yang mengizinkan:

| Situasi | Perilaku |
|---|---|
| `secrets.h` tidak ada | Jaringan nonaktif total, perangkat berjalan offline-only (bukan mencoba konek dengan kredensial kosong) |
| `BACKEND_EVENTS_ENDPOINT` http:// tanpa flag eksplisit | Upload dinonaktifkan (`esp32dev`) atau ditolak backend |
| `https://` tanpa root CA / insecure flag | Upload dinonaktifkan |
| NTP belum sinkron saat command diterima | Command ditolak (`isCommandValidNow` mengembalikan false untuk `nowUnix==0`) |
| HMAC signature tidak cocok | Command ditolak, TIDAK dieksekusi, dicatat `COMMAND_REJECTED` |
| Slot sidik jari di-revoke tapi sensor masih match fisik | Tetap `NO_MATCH` (access_policy dicek SETELAH sensor) |
| `ADMIN_API_KEY` tidak diset di backend | Proses gagal start (bukan diam-diam tanpa proteksi) |

### 3. Backend Failure ≠ Access Failure (SRS 13.6)

Backend mati total (atau Wi-Fi mati total) tidak pernah menghentikan
verifikasi sidik jari lokal. `ApiClient` dan `CommandClient` gagal secara
senyap (log + retry/backoff), tidak pernah melempar exception yang bisa
mengganggu `loop()` — karena keduanya memang berjalan di task terpisah
yang tidak disentuh `loop()` sama sekali.

### 4. Idempotensi di Semua Lapisan

- **Event ingestion**: kunci unik `(device_id, log_epoch, boot_id, sequence)`
  — retry firmware akibat response hilang di jaringan tidak pernah
  menggandakan baris di database.
- **Command execution**: `command_id` dilacak statusnya
  (`pending → acknowledged → executed/failed/expired`); ack kedua untuk
  command yang sudah final tidak mengubah apa pun lagi.
- **Backoff eksponensial** (`backoff.h`) dipakai konsisten oleh
  `NetworkManager` (reconnect Wi-Fi), `ApiClient` (retry upload), dan
  `CommandClient` (retry poll) — satu implementasi, diuji sekali.

## Batas Fase (Phase Boundary)

Phase 4 (remote user sync, command queue, remote unlock) dibangun **di
atas** Phase 3, bukan menggantikannya. Supaya branch `phase-3` (lihat
[[Contributing]]) bisa tetap ada dan compile tanpa kode Phase 4, batas ini
ditegakkan di DUA level sekaligus, bukan hanya didokumentasikan:

### Firmware

| Mekanisme | Detail |
|---|---|
| Lokasi fisik | Seluruh kode Phase 4 ada di `src/phase4/` (`access_policy.*`, `command_policy.*`, `command_client.*`). Phase 1-3 tetap di `src/` |
| Flag kompilasi | `ENABLE_PHASE4_FEATURES` (default `1`) di `include/config.h`. Semua pemanggilan API Phase 4 di `main.cpp`/`state_machine.cpp`/`fingerprint.cpp` dibungkus `#if ENABLE_PHASE4_FEATURES` |
| Arah dependensi | `src/phase4/*` BOLEH meng-`#include` apa pun dari `src/` (mis. `backoff.h`, `url_utils.h`), tapi **tidak sebaliknya** — tidak ada satu pun file di `src/` (selain yang dibungkus `#if`) yang mem-`#include` sesuatu dari `src/phase4/` tanpa guard |
| Pembuktian otomatis | Environment PlatformIO `esp32dev-phase3only` — `-D ENABLE_PHASE4_FEATURES=0` **dan** `build_src_filter` yang mengeluarkan `phase4/` sepenuhnya. Build ini jalan di CI pada setiap PR; jika pernah gagal, berarti ada pelanggaran arah dependensi |

### Backend

| Mekanisme | Detail |
|---|---|
| Lokasi fisik | `app/routers/phase3/` (events, devices, heartbeat) vs `app/routers/phase4/` (users, commands) |
| Flag runtime | `PHASE4_ENABLED` (env var, default `true`). `create_app()` di `app/main.py` adalah **application factory** — dipanggil ulang tiap kali instance baru dibutuhkan (bukan `app = FastAPI()` tunggal di top-level modul), supaya keputusan "router mana yang di-mount" selalu dibaca dari konfigurasi SAAT ITU, bukan dibekukan sejak modul pertama di-import |
| Pembuktian otomatis | `backend/tests/test_phase_boundary.py` — membuktikan `PHASE4_ENABLED=false` membuat `/api/v1/users` dan `/api/v1/commands*` mengembalikan **404** (route benar-benar tidak terdaftar), bukan 401/403 |

Dengan dua mekanisme ini, membuat branch `phase-3` yang murni tinggal:
menghapus `src/phase4/` dan `app/routers/phase4/`, set
`ENABLE_PHASE4_FEATURES=0` / `PHASE4_ENABLED=false` sebagai default — dan
CI akan tetap hijau karena arah dependensinya sudah diverifikasi sejak awal.

## Konsumsi Memori & Penyimpanan (ESP32)

| Komponen | Ukuran |
|---|---|
| `EventRecord` (ring buffer log) | 45 byte × 50 entri ≈ 2.2 KB (LittleFS) |
| `AccessPolicy` mask | 8 byte (NVS, bitmask 64 slot) |
| Command bridge (RAM) | ~150 byte (1 slot in-flight, statis) |
| Network task stack | 12 KB (`NET_TASK_STACK_BYTES`, dapat disesuaikan) |

Semua struktur berukuran tetap dan dialokasikan statis — tidak ada
alokasi dinamis tak terbatas yang bisa memicu heap fragmentation dalam
operasi jangka panjang (kecuali buffer JSON sementara saat serialisasi
HTTP, yang di-`free()` segera setelah request selesai).
