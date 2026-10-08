# Remote Commands (Phase 4)

Dashboard & Manajemen Akses Jarak Jauh: command queue, remote unlock, dan
sinkronisasi izin akses (ALLOW_SLOT/REVOKE_SLOT) dari backend ke firmware.

## Alur Lengkap

```
Admin (dashboard/API)                Backend                    Firmware (net_task)
       │                                │                              │
       │ POST /api/v1/commands          │                              │
       │ {device_id, command_type,      │                              │
       │  payload, ttl_seconds}          │                              │
       ├───────────────────────────────►│                              │
       │                          status=pending                       │
       │                                │                              │
       │                                │◄──── GET /commands/poll ─────┤  (tiap COMMAND_POLL_INTERVAL_MS)
       │                                │  Bearer + X-Device-Id        │
       │                                │                              │
       │                                │  tandatangani tiap command   │
       │                                │  dengan HMAC-SHA256          │
       │                                │  (device.hmac_secret)        │
       │                                │  status -> acknowledged      │
       │                                ├──────────────────────────────►
       │                                │  {commands:[{id, type,      │
       │                                │    payload, issued_at,       │
       │                                │    expires_at, signature}]}  │
       │                                │                              │
       │                                │                     verifikasi HMAC
       │                                │                     (command_policy.h)
       │                                │                     cek expires_at vs
       │                                │                     NTP (fail-closed
       │                                │                     jika belum sinkron)
       │                                │                              │
       │                                │                     ┌────────┴────────┐
       │                                │                     │ ALLOW/REVOKE_SLOT│  REMOTE_UNLOCK
       │                                │                     │ -> access_policy │  -> dititipkan ke
       │                                │                     │    (NVS, langsung)│    state machine
       │                                │                     └────────┬────────┘    (loop task)
       │                                │                              │                   │
       │                                │◄── POST .../ack {status} ────┤◄──────────────────┘
       │                                │  status -> executed/failed/  │  via bridge mutex
       │                                │            expired            │
```

## Tipe Command

| `command_type` | Payload | Efek di firmware |
|---|---|---|
| `REMOTE_UNLOCK` | `{}` | Dititipkan ke state machine lewat bridge; dieksekusi HANYA saat device di state `IDLE` (SRS 6.3) |
| `ALLOW_SLOT` | `{"slot": N}` | `AccessPolicy::setSlotAllowed(N, true)` — langsung di net_task, tidak perlu loop task |
| `REVOKE_SLOT` | `{"slot": N}` | `AccessPolicy::setSlotAllowed(N, false)` — sidik jari yang match fisik ke slot ini langsung jadi `NO_MATCH` |

## Verifikasi Command (Sebelum Eksekusi)

Setiap command yang diterima dari `GET /commands/poll` melewati **tiga**
pemeriksaan berurutan di `command_client.cpp`, memakai fungsi murni dari
`command_policy.h` (diuji native, lihat [[Testing]]):

1. **Kelengkapan field** — `id`, `command_type`, `signature` tidak boleh kosong.
2. **Signature HMAC-SHA256** — dihitung ulang di firmware dari
   `"<id>.<command_type>.<issued_at>.<expires_at>.<canonical_payload>"`
   memakai `DEVICE_HMAC_SECRET`, dibandingkan **constant-time**
   (`constantTimeHexEqual`) terhadap signature dari server.
3. **Masa berlaku** — `isCommandValidNow(nowUnix, expiresAt)`. Jika NTP
   belum sinkron (`nowUnix == 0`), **selalu ditolak** (fail-closed) —
   tidak pernah mengeksekusi berdasarkan asumsi waktu yang tidak diketahui.

Command yang gagal verifikasi di-ack `failed`/`expired` dengan alasannya,
dan dicatat sebagai event lokal `COMMAND_REJECTED` — operator dapat
melihatnya lewat `w` atau di tabel Access Logs dashboard.

## Mengapa HMAC Terpisah dari Bearer Token?

Bearer token (`DEVICE_API_TOKEN`) mengautentikasi **siapa yang bicara**
(device → backend). HMAC (`DEVICE_HMAC_SECRET`) membuktikan **isi pesan
tidak diubah** di tengah jalan (backend → device). Dua kunci terpisah
berarti kebocoran satu kredensial tidak otomatis membahayakan yang lain —
lihat [[Security]] untuk model ancaman lengkap.

## Bridge REMOTE_UNLOCK (Cross-Task)

`REMOTE_UNLOCK` **tidak pernah** dieksekusi langsung dari `net_task`
(yang tidak memegang kontrol aktuator). Sebagai gantinya:

1. `CommandClient` (net_task) memverifikasi command, lalu memanggil
   `consumeUnlockRequest()`-compatible slot: menyimpan `command_id` di
   satu slot bermutex (`_unlockPending`).
2. `StateMachine::runIdle()` (loop task) memanggil
   `commandClient.consumeUnlockRequest(...)` di **setiap tick** — jika
   ada permintaan pending, langsung transisi ke `UNLOCKED` (melewati
   `VERIFYING`, karena otorisasi sudah final di langkah HMAC di atas).
3. Event `REMOTE_UNLOCK` dicatat (bukan `ACCESS_GRANTED`) — log tetap
   bisa membedakan akses biometrik vs. perintah jarak jauh.
4. `reportUnlockResult()` memberi tahu `CommandClient` hasilnya;
   `net_task` mengirim ack `executed` ke backend pada tick berikutnya.

**Satu slot in-flight**: command `REMOTE_UNLOCK` kedua yang datang saat
slot masih terisi langsung di-ack `failed` ("another remote unlock
already pending"). **Auto-expiry**: jika device tidak pernah mencapai
`IDLE` (mis. macet di `ERROR_SAFE`) sebelum `expires_at`,
`checkUnlockBridgeExpiry()` di net_task melaporkan `expired` ke backend
tanpa menunggu selamanya.

## Sinkronisasi Akses (`AccessPolicy`)

- Bitmask 64-bit di NVS (`ACCESS_POLICY_NVS_NAMESPACE`), satu bit per slot
  sidik jari (0-63).
- **Default semua slot diizinkan** (`mask = ~0`) — supaya device yang
  firmware-nya di-upgrade dari Phase 1-3 ke Phase 4 tidak tiba-tiba
  mengunci semua orang hanya karena belum pernah menerima command apa pun.
- Dicek **setelah** sensor menyatakan match (`fingerprint.cpp`) — revoke
  tidak menghapus template dari sensor fisik, hanya menambah lapisan
  otorisasi. Ini konsisten dengan **Local-First** (SRS 7.4): penerapan
  revocation tidak butuh koneksi backend saat kejadian akses berlangsung,
  cukup mask yang sudah tersinkron sebelumnya.

## Backend: Pemicu Otomatis dari Credential Management

`POST /api/v1/users/{id}/credentials` dan
`DELETE /api/v1/users/{id}/credentials/{cred_id}` (lihat
[[API-Reference]]) **otomatis membuat command** `ALLOW_SLOT`/`REVOKE_SLOT`
— admin tidak perlu membuat command secara manual terpisah saat
mendaftarkan atau mencabut kredensial pengguna.

## Bench-Test End-to-End

```bash
# Terminal 1: mock backend dengan dukungan command
python tools/mock_backend.py --token dev-token-123 --hmac-secret dev-hmac-456

# Terminal 2: firmware, secrets.h diisi token+hmac yang sama di atas
pio run -e esp32dev-bench --target upload && pio device monitor

# Terminal 3: simulasikan "operator menekan tombol Remote Unlock"
python tools/mock_backend.py --enqueue REMOTE_UNLOCK --device DOOR-01

# Variasi lain:
python tools/mock_backend.py --enqueue ALLOW_SLOT --device DOOR-01 --slot 3
python tools/mock_backend.py --enqueue REVOKE_SLOT --device DOOR-01 --slot 1
```

| # | Skenario | Hasil yang diharapkan |
|---|---|---|
| 1 | Enqueue `REMOTE_UNLOCK` saat device `IDLE` | Dalam `COMMAND_POLL_INTERVAL_MS`, pintu `UNLOCKED`; Serial: "diterima & valid -> dititipkan"; mock backend terima ack `executed` |
| 2 | Enqueue `REMOTE_UNLOCK` saat device di `ERROR_SAFE` (simulasikan sensor gagal) | Command TIDAK dieksekusi; setelah `ttl_seconds` habis, ack `expired` otomatis terkirim |
| 3 | Enqueue `REVOKE_SLOT --slot 1`, lalu tekan tombol mock (selalu slot #1) | Firmware: "Slot #1 cocok di sensor TAPI sudah di-revoke -> NO MATCH"; `ACCESS_DENIED` tercatat, bukan `ACCESS_GRANTED` |
| 4 | Enqueue `ALLOW_SLOT --slot 1` (balikkan skenario 3) | Slot #1 kembali diterima normal |
| 5 | Ubah 1 karakter `DEVICE_HMAC_SECRET` di firmware (tidak cocok dengan server) | Semua command ditolak: "*** SIGNATURE TIDAK VALID ***"; `COMMAND_REJECTED` tercatat; ack `failed` dikirim |
| 6 | Dua `REMOTE_UNLOCK` diantre berurutan cepat | Yang kedua di-ack `failed` ("already pending") sebelum yang pertama selesai |

Detail keamanan lebih dalam: [[Security]]. Referensi payload lengkap:
[[API-Reference]].
