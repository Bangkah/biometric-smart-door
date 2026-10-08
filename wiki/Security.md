# Security

Model keamanan lintas firmware ↔ backend (SRS Bab 12, NFR-001).

## Dua Kredensial Terpisah per Perangkat

| Kredensial | Arah | Fungsi | Disimpan backend sebagai |
|---|---|---|---|
| `DEVICE_API_TOKEN` (Bearer) | Device → Backend | Autentikasi: "siapa yang bicara" | SHA-256 hash (`api_token_hash`) |
| `DEVICE_HMAC_SECRET` | Backend → Device | Integritas: "command ini sah, tidak diubah di tengah jalan" | Plaintext (dipakai backend untuk menandatangani, bukan untuk verifikasi masuk) |

Kenapa dipisah? Kebocoran satu kredensial (mis. token Bearer ter-log di
proxy HTTP) tidak otomatis memungkinkan penyerang memalsukan command
`REMOTE_UNLOCK` yang valid — ia masih butuh `DEVICE_HMAC_SECRET` yang
tidak pernah dikirim lewat header/log yang sama.

Backend **tidak pernah** menyimpan token Bearer dalam bentuk yang bisa
dibaca ulang — hanya hash SHA-256-nya. Token asli hanya muncul sekali,
di response `POST /api/v1/devices`.

## Kebijakan Transport (TLS, SRS NFR-001)

`ApiClient::begin()` dan `CommandClient::begin()` **fail-closed**:

```
BACKEND_EVENTS_ENDPOINT dimulai http://  + BACKEND_ALLOW_PLAIN_HTTP=0 (default)
  -> modul jaringan terkait DINONAKTIFKAN total (bukan fallback diam-diam)

BACKEND_EVENTS_ENDPOINT dimulai https:// + BACKEND_ROOT_CA kosong
  + BACKEND_TLS_INSECURE=0 (default)
  -> DINONAKTIFKAN (TLS tanpa verifikasi sertifikat butuh opt-in eksplisit)
```

`http://` hanya diizinkan lewat environment build terpisah
(`esp32dev-bench`, `-D BACKEND_ALLOW_PLAIN_HTTP=1`) — untuk
`tools/mock_backend.py` di LAN tepercaya saja, **tidak pernah** di build
produksi (`esp32dev`).

## Verifikasi Command (Anti-Pemalsuan & Anti-Replay)

Lihat [[Remote-Commands]] untuk alur lengkap. Ringkasan pertahanan:

| Ancaman | Pertahanan |
|---|---|
| Command dipalsukan di jaringan (MITM, DNS-spoof) | HMAC-SHA256 per-command, kunci terpisah dari Bearer token |
| Command lama diputar ulang setelah valid | `expires_at` + `isCommandValidNow()` — TTL pendek (default 60-120 detik) |
| Device dieksploitasi sebelum NTP sinkron | `nowUnix == 0` **selalu** dianggap tidak valid (fail-closed), bukan "izinkan karena tidak bisa dicek" |
| Timing attack saat membandingkan signature | `constantTimeHexEqual()` (firmware) / `hmac.compare_digest()` (backend) — bukan `strcmp`/`==` biasa |
| Command dieksekusi dua kali (retry jaringan) | `command_id` dilacak statusnya; ack kedua pada command final tidak mengubah apa pun |
| Dua string signature C++ vs Python tidak pernah cocok | Diuji SILANG: `backend/tests/test_security.py` dan `test/test_command_policy` memakai **vektor uji identik**, dicek byte-per-byte sama |

## Fail-Safe vs Fail-Closed

- **Fail-safe** (mekanis, Phase 1): kegagalan hardware/power selalu
  berakhir di posisi **terkunci** (SRS 13.4/13.5).
- **Fail-closed** (otorisasi, Phase 3-4): kegagalan verifikasi (token,
  HMAC, waktu tidak diketahui) selalu berakhir di **penolakan**, bukan
  izin default.

Kedua prinsip ini tidak pernah saling mengorbankan satu sama lain:
backend/jaringan mati total → akses lokal tetap berfungsi (SRS 13.6);
command tidak terverifikasi → tidak pernah dieksekusi, walau itu berarti
operator harus mengirim ulang.

## Admin Authentication

`X-Admin-Key` dicocokkan dengan `ADMIN_API_KEY` dari environment
(`hmac.compare_digest`, constant-time). Backend **menolak untuk start**
bila `ADMIN_API_KEY` tidak diset — mencegah deployment yang diam-diam
tanpa proteksi admin sama sekali.

> **Catatan skala proyek saat ini**: satu admin key statis cukup untuk
> Phase 4 (single operator / tim kecil). Sistem user/role admin
> bertingkat adalah kandidat penguatan di luar cakupan SRS saat ini.

## Yang BELUM Dicakup (Batasan Jujur)

- **Token dinamis via endpoint Device Authentication (SRS 10.1)** — Phase
  4 masih memakai token statis per device yang dibuat sekali saat
  registrasi. Rotasi/refresh token otomatis adalah penguatan Phase 5/6.
- **Rate limiting** pada endpoint publik — belum diterapkan; untuk
  deployment produksi nyata, pasang di reverse proxy (nginx `limit_req`,
  Cloudflare, dst).
- **Audit log untuk aksi admin sendiri** (siapa membuat command apa,
  kapan) — saat ini hanya `access_logs` (aktivitas perangkat), belum ada
  tabel audit terpisah untuk aktivitas dashboard/admin.
