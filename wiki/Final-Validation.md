# Final Validation — Acceptance Criteria & Definition of Done

**Tanggal validasi:** 2026-10-06
**Lingkup:** SRS Bab 21 (Acceptance Criteria), Bab 22 (Definition of Done),
ditelusuri balik ke FR-001–FR-020 dan NFR-001–NFR-003 (SRS Bab 15-16).
**Status proyek saat validasi:** Phase 1-4 selesai software; hardware
integration & deployment produksi **belum** dilakukan (lihat Metodologi).

---

## Metodologi & Batasan (baca ini dulu)

Validasi ini dijalankan di lingkungan pengembangan **tanpa hardware ESP32
fisik dan tanpa deployment backend produksi**. Setiap klaim di bawah
diberi label jujur:

| Label | Arti |
|---|---|
| ✅ **Terverifikasi** | Ada test otomatis yang BENAR-BENAR DIJALANKAN (bukan cuma syntax-check) dan lulus, ATAU pembuktian arsitektural yang kuat (mis. compile-check negatif) |
| ⚠️ **Terverifikasi Sebagian** | Ada bukti pendukung (code review, simulasi, bench-test manual terdokumentasi) tapi BUKAN test otomatis end-to-end di kondisi final |
| ❌ **Belum Terverifikasi / Gap** | Tidak ada bukti; teridentifikasi sebagai pekerjaan tersisa |

**Yang TIDAK bisa divalidasi di sini** (butuh hardware/deployment nyata —
ini tepat yang akan dikerjakan di tahap hardware integration berikutnya):
waktu verifikasi sidik jari sungguhan pada sensor AS608 fisik, perilaku
aktuator (servo/solenoid) sungguhan, stabilitas daya 12V nyata, dan
performa jaringan Wi-Fi di lokasi pemasangan sesungguhnya.

Seluruh log mentah yang dikutip di bawah dihasilkan dari run test yang
sama pada **2026-10-06 23:4x UTC**, tersimpan sebagai evidence untuk
dokumen ini.

---

## 1. Acceptance Criteria (SRS Bab 21)

### 21.1 — "Perangkat mampu memverifikasi sidik jari dan membuka kunci secara lokal saat sidik jari valid dikenali tanpa koneksi internet."

**Status: ⚠️ Terverifikasi Sebagian**

| Bukti | Hasil |
|---|---|
| State machine lokal (`BOOT→...→IDLE→VERIFYING→UNLOCKED`) | ✅ `test_event_logger` (9 test) memvalidasi siklus event yang menyertai transisi ini |
| Independensi dari jaringan | ✅ **Dibuktikan negatif**: `src/*.cpp` (8 file: `main`, `state_machine`, `fingerprint`, `actuator`, `event_logger`, `network_manager`, `time_sync`, `api_client`) berhasil di-compile dengan `ENABLE_PHASE4_FEATURES=0` **dan tanpa folder `src/phase4/` di include path sama sekali** — 16/16 kompilasi bersih (2 varian Arduino-ESP32 core × 8 file) |
| Timeout verifikasi < 2 detik (FR-001) | ⚠️ Ceiling diberlakukan via `FP_VERIFY_TIMEOUT_MS=2000` di `fingerprint.cpp` (dibuktikan ada di kode), TAPI waktu verifikasi SUNGGUHAN pada sensor fisik belum diukur (butuh hardware) |
| Unlock-auto-lock pada hardware fisik | ❌ Belum — hanya diverifikasi lewat Serial log & logika kode, actuator fisik belum pernah digerakkan dalam sesi ini |

**Kesimpulan**: logika dan independensi-jaringan terverifikasi kuat
secara statis; **perilaku real-time pada hardware fisik masih gap**,
sesuai arah kerja berikutnya (hardware integration).

---

### 21.2 — "Log akses berhasil disimpan dalam persistent buffer, dikirim, dan ditampilkan di dasbor web saat perangkat terhubung kembali ke jaringan."

**Status: ✅ Terverifikasi (end-to-end, via simulasi)**

| Tahap | Bukti |
|---|---|
| Disimpan di persistent buffer | ✅ `test_event_logger::test_events_survive_reboot` — instance `EventLogger` baru di atas "flash" yang sama membuktikan data selamat dari reboot |
| Tahan power-loss di tengah operasi | ✅ `test_event_logger::test_ring_buffer_wraps_at_capacity_discarding_oldest` + desain self-healing (`recoverStateFromFlash` dari `sequence` tertinggi, bukan file metadata terpisah) |
| Dikirim ke backend (idempoten) | ✅ `backend/tests/test_events.py` (7 test): `test_ingest_events_accepts_new_batch`, `test_ingest_events_is_idempotent_on_retry`, `test_ingest_events_partial_overlap_only_new_accepted`, `test_ingest_events_different_epoch_not_deduped` — SEMUA PASSED |
| Ditampilkan di dashboard | ✅ `test_phase_boundary.py::test_dashboard_served_at_root` (HTML benar-benar ter-serve) + `dashboard.html` memanggil `GET /api/v1/devices/{id}/logs` (diverifikasi via `test_devices.py::test_device_status_online_after_event`, endpoint yang sama) |
| End-to-end dengan backend sungguhan (bukan TestClient in-process) | ⚠️ Diverifikasi via `tools/mock_backend.py` dijalankan sungguhan sebagai proses HTTP terpisah (lihat log fungsional sebelumnya di percakapan ini: enqueue→poll→ack→dedup semua lulus), **BUKAN** via firmware fisik yang benar-benar mengirim HTTP melalui Wi-Fi nyata |

**Kesimpulan**: alur data tervalidasi penuh di level logika +
HTTP-sungguhan (mock backend), gap tersisa murni di lapisan "firmware
fisik berbicara ke backend lewat radio Wi-Fi sungguhan".

---

### 21.3 — "Fitur remote unlock dari dasbor berhasil menggerakkan aktuator perangkat setelah melalui validasi keamanan command_id, masa kedaluwarsa, dan perilaku idempotent."

**Status: ⚠️ Terverifikasi Sebagian (logika penuh; aktuator fisik belum)**

| Sub-klaim | Bukti |
|---|---|
| Validasi `command_id` unik + idempotensi ack | ✅ `backend/tests/test_commands.py::test_ack_is_idempotent_after_terminal_status`, `test_device_cannot_ack_other_devices_command` |
| Masa kedaluwarsa (TTL) ditolak setelah lewat | ✅ `test_commands.py::test_expired_command_not_returned_by_poll` (test nyata dengan `time.sleep(1.2)`, bukan mock waktu) + `test_command_policy.cpp::test_command_invalid_when_expired` (firmware, 2 skenario batas) |
| Signature HMAC mencegah command dipalsukan | ✅ `test_commands.py::test_allow_slot_and_revoke_slot_signature_covers_payload` (backend) **DAN** `test_command_policy.cpp` (firmware) — **vektor uji identik di kedua bahasa, dibuktikan byte-per-byte sama** (cross-check manual tercatat di sesi sebelumnya) |
| Command TIDAK dieksekusi bila NTP belum sinkron | ✅ `test_command_policy.cpp::test_command_invalid_when_time_unknown` — fail-closed untuk `nowUnix==0` |
| Bridge lintas-task (net_task → loop task) aman | ⚠️ Diverifikasi via code review + compile-check (`command_client.cpp`/`state_machine.cpp` saling memanggil API bridge dengan tipe yang konsisten), **TIDAK ada test otomatis** yang mensimulasikan race condition sungguhan antar-task FreeRTOS (sulit diuji murni di host) |
| End-to-end: enqueue → poll → eksekusi → ack | ✅ Dijalankan sungguhan via `mock_backend.py` dalam sesi ini: command ter-enqueue, firmware (disimulasikan via klien HTTP Python) berhasil poll, signature tervalidasi cocok, ack `executed` diterima — lihat log fungsional di atas |
| **Aktuator fisik benar-benar bergerak** | ❌ **Belum** — `actuatorModule.unlock()` hanya diverifikasi lewat Serial log ("[ACTUATOR] -> UNLOCKED") dan review kode, bukan observasi relay/servo fisik |

**Kesimpulan**: ini kriteria dengan cakupan test PALING KUAT dari
ketiganya (security-critical path diuji berlapis, termasuk cross-language
verification) — tapi klaim "menggerakkan aktuator perangkat" secara
harfiah baru terverifikasi sampai batas Serial log, bukan observasi fisik.

---

## 2. Definition of Done (SRS Bab 22)

### 22.1 — "Kode program telah melalui code review dan pengujian unit/integrasi."

**Status: ⚠️ Terverifikasi Sebagian**

- ✅ Pengujian unit/integrasi: **96 test otomatis**, seluruhnya lulus pada
  run hari ini (50 backend + 46 firmware native, rincian di §3).
- ✅ Lint bersih: `ruff check backend/` → "All checks passed!"; seluruh
  firmware `.cpp` di-compile dengan `-Wall -Wextra` tanpa warning.
- ❌ **Code review manusia kedua**: belum terjadi — seluruh kode ditulis
  dan diverifikasi oleh satu agen (saya) lewat kompilasi/test berulang,
  BUKAN oleh reviewer manusia independen. Ini gap proses, bukan gap teknis.

### 22.2 — "Dokumentasi API dan panduan instalasi perangkat keras telah tersedia lengkap."

**Status: ✅ Terverifikasi**

| Dokumen | Lokasi |
|---|---|
| Referensi API lengkap (request/response semua endpoint) | `wiki/API-Reference.md` |
| Panduan instalasi hardware (wiring, pin mapping, power) | `wiki/Firmware-Setup.md` §Kebutuhan Perangkat Keras, §Wiring |
| Panduan setup backend | `wiki/Backend-Setup.md` |
| Swagger UI otomatis (live, bukan statis) | `backend/app/main.py` — FastAPI generate `/docs` otomatis dari `schemas.py` |

### 22.3 — "Fitur lulus seluruh kriteria penerimaan (Acceptance Criteria) pada lingkungan pengujian akhir."

**Status: ❌ Belum Terverifikasi**

Per §1 di atas, ketiga Acceptance Criteria lulus **di lingkungan
pengembangan/simulasi**, bukan "lingkungan pengujian akhir" (hardware
fisik + backend ter-deploy). Item ini **secara definisi** baru bisa
ditutup setelah tahap hardware integration yang akan datang selesai.

---

## 3. Traceability Matrix — Functional & Non-Functional Requirements

### Functional Requirements

| ID | Requirement (ringkas) | Status | Bukti / Lokasi |
|---|---|---|---|
| FR-001 | Verifikasi lokal < 2 detik | ⚠️ | `FP_VERIFY_TIMEOUT_MS=2000` (`config.h:63`); ceiling di kode, belum diukur di hardware |
| FR-002 | Pendaftaran kredensial **lokal di perangkat** | ❌ **GAP** | `fingerprint.cpp` tidak punya fungsi `createModel`/`storeModel` (enrollment di sensor). Hanya ADA jalur remote (`ALLOW_SLOT` dari backend), bukan enrollment on-device murni sesuai teks FR-002 |
| FR-003 | Tolak sidik jari tak dikenal | ✅ | `test_event_logger` (ACCESS_DENIED path), `fingerprint.cpp` NO_MATCH handling |
| FR-004 | Unlock saat match valid | ✅ | `state_machine.cpp::runUnlocked()`, diuji via `test_event_logger` |
| FR-005 | Auto-lock setelah timeout | ✅ | `UNLOCK_DURATION_MS`, `runUnlocked()` |
| FR-006 | Operasi mandiri saat offline | ✅ | Compile-check negatif §Metodologi; `esp32dev-phase3only` |
| FR-007 | Struktur data event lokal | ✅ | `EventRecord` (`event_logger.h`), `static_assert(sizeof==45)` |
| FR-008 | Persistent buffer lokal | ✅ | Ring buffer LittleFS, `test_event_logger` (9 test) |
| FR-009 | Sinkronisasi event ke backend | ✅ | `api_client.cpp` + `backend/tests/test_events.py` |
| FR-010 | Manajemen data pengguna di dashboard | ✅ | `routers/phase4/users.py` + `dashboard.html` + `test_users.py` |
| FR-011 | Manajemen status perangkat keras | ✅ | `Device.status()`, `test_devices.py` |
| FR-012 | Pemetaan kredensial per device+slot | ✅ | `UniqueConstraint(device_id, sensor_slot_id)`, `test_duplicate_slot_on_same_device_conflicts` |
| FR-013 | Revocation dengan sinkronisasi lokal | ✅ | `REVOKE_SLOT` command otomatis, `access_policy.cpp`, `test_revoke_credential_auto_creates_revoke_slot_command` |
| FR-014 | Catat & tampilkan log akses | ✅ | `access_logs` table, `GET /devices/{id}/logs`, dashboard |
| FR-015 | Heartbeat periodik | ✅ | `POST /devices/heartbeat` terpisah dari event sync, `test_heartbeat.py` (4 test) |
| FR-016 | Siklus hidup enrollment terstruktur (PENDING→WAITING_FOR_SCAN→PROCESSING→COMPLETED) | ❌ **GAP** | `CredentialStatus` backend hanya `active`/`revoked` — TIDAK ada state machine 4-tahap sesuai SRS 7.3. Kredensial langsung `active` begitu di-POST |
| FR-017 | Remote unlock idempotent | ✅ | §1 (21.3) di atas |
| FR-018 | Acknowledgement eksekusi command | ✅ | `POST /commands/{id}/ack`, `test_device_ack_updates_status` |
| FR-019 | Tolak command kedaluwarsa | ✅ | §1 (21.3), `test_expired_command_not_returned_by_poll` |
| FR-020 | Audit trail tidak terhapus | ⚠️ | Tidak ada endpoint DELETE untuk `access_logs` (benar secara arsitektur), TAPI **tidak ada test eksplisit** yang mencoba menghapus dan mengharapkan penolakan |

### Non-Functional Requirements

| ID | Requirement | Status | Bukti / Lokasi |
|---|---|---|---|
| NFR-001 | Seluruh komunikasi wajib TLS | ✅ | Kebijakan fail-closed `ApiClient::begin()`/`CommandClient::begin()`; `BACKEND_ALLOW_PLAIN_HTTP` hanya di `esp32dev-bench` |
| NFR-002 | Verifikasi lokal tidak bergantung jaringan | ✅ | Arsitektur network task terpisah + compile-check negatif (§Metodologi) |
| NFR-003 | Skalabilitas puluhan device | ❌ **GAP** | Tidak ada load/stress test. Desain mendukung (index pada `device_id`, query terbatas `limit`), tapi klaim **belum diuji secara empiris** |

**Ringkasan traceability**: 15 ✅ Terverifikasi, 3 ⚠️ Sebagian, 3 ❌ Gap
(FR-002, FR-016, NFR-003), dari 20 FR + 3 NFR.

---

## 4. Gap Register (Prioritas untuk Tahap Berikutnya)

| # | Gap | Dampak | Rekomendasi |
|---|---|---|---|
| 1 | FR-002/FR-016: enrollment sidik jari baru masih manual via tool vendor sensor, tidak ada alur terstruktur di firmware/backend | Sedang — operasional masih bisa jalan (admin enroll manual lalu `POST /credentials`), tapi tidak sesuai teks SRS persis | Jika SRS dibaca ketat, ini PR terpisah: tambah state machine enrollment 4-tahap + command baru `START_ENROLLMENT` yang memicu firmware masuk mode `createModel`/`storeModel` sensor |
| 2 | Hardware fisik belum pernah diuji sama sekali dalam proyek ini | **Tinggi** — seluruh Acceptance Criteria bergantung pada ini | Tepat sasaran tahap "hardware integration" yang akan kamu kerjakan berikutnya |
| 3 | NFR-003 tidak ada load test | Rendah untuk skala saat ini (1 device dalam semua test), tapi klaim "puluhan device" di SRS belum dibuktikan | Tambah test dengan 20-50 device registrasi paralel + burst event ingestion, ukur response time |
| 4 | FR-020 tidak ada test eksplisit (hanya arsitektur) | Rendah | Tambah test singkat: pastikan tidak ada route `DELETE /api/v1/devices/{id}/logs` terdaftar (404) |
| 5 | Code review manusia kedua belum terjadi (DoD 22.1) | Proses, bukan teknis | Perlu dijadwalkan terpisah dari pekerjaan otomatis ini |

---

## 5. Bukti Mentah (Ringkasan Angka)

```
Backend  : 50 passed, 0 failed, 1 warning (non-blocking, dependensi pihak ketiga)
           Coverage: 98% (554 statements, 10 miss)
Firmware : 46 passed, 0 failed
           test_backoff(5) + test_time_estimate(5) + test_url_utils(5)
           + test_event_logger(9) + test_access_policy(7) + test_command_policy(15)
Compile  : 22/22 bersih (PHASE4=1, 11 file × 2 varian core)
           16/16 bersih (PHASE4=0, TANPA folder phase4/, 8 file × 2 varian core)
Lint     : ruff — All checks passed
-----------------------------------------------------------
TOTAL TEST OTOMATIS LULUS: 96/96
TOTAL KOMPILASI BERSIH   : 38/38
```

---

## 6. Sign-off

| Peran | Nama | Tanggal | Catatan |
|---|---|---|---|
| Penyusun laporan (otomatis) | Claude (agen) | 2026-10-06 | Seluruh angka di atas dari run test SUNGGUHAN pada tanggal ini, bukan klaim tanpa verifikasi |
| Reviewer kode (manusia) | *(belum diisi)* | | Mengisi ini menutup gap DoD 22.1 |
| Penguji hardware | *(belum diisi)* | | Mengisi ini menutup gap AC 21.1-21.3 kolom "hardware fisik" |
| Acceptance sign-off final | *(belum diisi)* | | Baru bisa ditandatangani setelah dua baris di atas terisi — lihat DoD 22.3 |
