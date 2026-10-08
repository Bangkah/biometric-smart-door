# API Reference

Base URL: `http://localhost:8000` (lokal) atau `https://backend-anda.example.com` (produksi).

Dua model autentikasi berbeda — **jangan tertukar**:

| | Header | Dipakai oleh | Endpoint |
|---|---|---|---|
| **Device auth** | `Authorization: Bearer <DEVICE_API_TOKEN>` + `X-Device-Id: <id>` | Firmware ESP32 | `/api/v1/events`, `/api/v1/devices/heartbeat`, `/api/v1/commands/poll`, `/api/v1/commands/{id}/ack` |
| **Admin auth** | `X-Admin-Key: <ADMIN_API_KEY>` | Dashboard / operator | `/api/v1/devices` (POST/GET), `/api/v1/devices/{id}/logs`, `/api/v1/users*`, `/api/v1/commands` (POST/GET, bukan `/poll`) |

---

## Phase 3 — Device Communication

### `POST /api/v1/events`

Ingestion log dari firmware (`api_client.cpp`). **Idempoten** berdasarkan
`(device_id, log_epoch, boot_id, sequence)`.

<details><summary>Request</summary>

```json
{
  "device_id": "DOOR-01",
  "firmware_version": "0.4.0-phase4",
  "log_epoch": 1,
  "sent_at": 1735689700,
  "missed_events": 0,
  "events": [
    {
      "sequence": 1, "boot_id": 3, "uptime_ms": 1234,
      "timestamp": 1735689700, "time_source": "ntp",
      "type": "ACCESS_GRANTED", "detail": 1, "message": "Fingerprint matched"
    }
  ]
}
```
</details>

<details><summary>Response 201</summary>

```json
{"accepted": 1, "duplicates": 0, "last_sequence": 1}
```
</details>

### `POST /api/v1/devices/heartbeat`

Telemetry periodik, **independen** dari event sync (lihat [[Architecture]]).

```json
// Request
{"firmware_version": "0.4.0-phase4", "uptime_ms": 123456, "free_heap_bytes": 180000}
// Response 200
{"status": "ok", "server_time": 1735689712}
```

### `POST /api/v1/devices` *(admin)*

Daftarkan perangkat baru. **Token & HMAC secret hanya ditampilkan sekali.**

```json
// Request
{"id": "DOOR-01", "name": "Pintu Depan", "location": "Lobby"}
// Response 201
{"id": "DOOR-01", "api_token": "a1b2...", "hmac_secret": "9f8e..."}
```

`409 Conflict` bila `id` sudah terdaftar.

### `GET /api/v1/devices` *(admin)*

```json
[{"id": "DOOR-01", "name": "Pintu Depan", "location": "Lobby",
  "firmware_version": "0.4.0-phase4", "last_seen": "2026-...", "status": "online",
  "last_log_epoch": 1}]
```

`status` dihitung dinamis dari `last_seen` (online jika < 5 menit lalu).

### `GET /api/v1/devices/{device_id}/logs?limit=100` *(admin)*

Riwayat `access_logs` untuk satu perangkat, terbaru dulu.

---

## Phase 4 — Remote User Sync & Commands

Seluruh endpoint di bawah **tidak terdaftar sama sekali** (404) bila
`PHASE4_ENABLED=false` — lihat [[Architecture]].

### `POST /api/v1/users` *(admin)*

```json
{"name": "Budi Santoso"}
```

### `POST /api/v1/users/{user_id}/credentials` *(admin)*

Mendaftarkan slot sidik jari untuk user pada device tertentu. **Otomatis
membuat command `ALLOW_SLOT`** (lihat [[Remote-Commands]]).

```json
// Request
{"device_id": "DOOR-01", "sensor_slot_id": 3, "push_command": true}
```

`409 Conflict` bila slot itu di device itu sudah dipakai kredensial lain.

### `DELETE /api/v1/users/{user_id}/credentials/{credential_id}` *(admin)*

Mencabut kredensial. **Otomatis membuat command `REVOKE_SLOT`.** Idempoten
— memanggil dua kali tidak membuat command ganda.

### `POST /api/v1/commands` *(admin)*

```json
{"device_id": "DOOR-01", "command_type": "REMOTE_UNLOCK", "payload": {}, "ttl_seconds": 60}
```

`command_type`: `REMOTE_UNLOCK` | `ALLOW_SLOT` | `REVOKE_SLOT`. Untuk dua
yang terakhir, `payload` wajib `{"slot": <int>}`. `ttl_seconds`: 1-3600.

### `GET /api/v1/commands?device_id=` *(admin)*

Daftar command (default 200 terbaru), untuk memantau status dari dashboard.

### `GET /api/v1/commands/poll` *(device)*

Dipanggil firmware tiap `COMMAND_POLL_INTERVAL_MS`. Mengembalikan command
`pending` milik device ini, **menandatanganinya dengan HMAC**, dan
menandainya `acknowledged`.

```json
{"commands": [{
  "id": "c1a2...", "command_type": "REMOTE_UNLOCK", "payload": {},
  "issued_at": 1735689700, "expires_at": 1735689760,
  "signature": "ab12...64-hex-chars"
}]}
```

### `POST /api/v1/commands/{command_id}/ack` *(device)*

```json
{"status": "executed", "detail": "unlocked"}
```

`status`: `executed` | `failed` | `expired`. Idempoten — ack kedua untuk
command yang sudah final tidak mengubah apa pun.

---

## Format Umum

- Semua timestamp epoch dalam **detik UTC** (bukan milidetik), kecuali
  disebut eksplisit (`uptime_ms`).
- Error mengikuti format FastAPI standar: `{"detail": "pesan" }` (404/401)
  atau `{"detail": [{"loc": [...], "msg": "...", ...}]}` (422 validasi Pydantic).
- `GET /health` tidak perlu autentikasi — `{"status": "ok", "phase4_enabled": true}`.

Swagger UI interaktif selalu tersedia di `/docs` saat server berjalan.
