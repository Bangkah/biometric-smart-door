# Software Requirements Specification (SRS)
## Biometric Smart Key & Management System

| | |
|---|---|
| **Document Status** | Final & Frozen (Contract for Implementation) |
| **Document Type** | Complete System Specification |
| **Product Lifecycle** | Single-Project / Final Build |

---

## 1. Introduction

### 1.1 Purpose

Dokumen ini mendefinisikan spesifikasi fungsional dan non-fungsional secara mutlak dan menyeluruh untuk pengembangan Biometric Smart Key & Management System. Dokumen ini bertindak sebagai satu-satunya sumber kebenaran (*single source of truth*) bagi pengembang perangkat keras, firmware, backend, frontend, dan penguji dalam membangun sistem hingga tuntas.

### 1.2 Scope

Cakupan sistem ini bersifat definitif dan mencakup:

- **Perangkat Keras & Firmware (Edge):** Pengoperasian mandiri berbasis mikrokontroler ESP32, sensor sidik jari biometrik, aktuator pengunci mekanis, buffer penyimpanan persisten, dan penanganan status lokal.
- **Sistem Backend & Database Relasional:** API terpusat untuk pengelolaan perangkat, sinkronisasi kredensial, penerimaan log telemetri, dan manajemen antrean perintah ber-ID.
- **Antarmuka Web Dashboard:** Panel kontrol administrator untuk pemantauan, manajemen pengguna, audit log, dan eksekusi perintah jarak jauh.

### 1.3 Objectives

- Membangun sistem kontrol akses berbasis biometrik yang mandiri (*local-first*), andal, dan aman.
- Menyediakan manajemen perangkat yang terpusat melalui web dashboard.
- Menjamin privasi data biometrik dengan menyimpan referensi template secara lokal di perangkat keras tanpa mengekspos data mentah ke server.

### 1.4 Target Users

- **End User / Anggota:** Pengguna yang mengakses pintu atau perangkat menggunakan verifikasi sidik jari terdaftar.
- **Administrator:** Pengguna yang memiliki hak akses penuh ke dasbor web untuk mengelola pengguna, mendaftarkan kredensial, memantau log akses, dan mengontrol perangkat.

### 1.5 Definitions

| Istilah | Definisi |
|---|---|
| **Edge Device** | Perangkat keras fisik (ESP32 dan komponen pendukungnya) yang terpasang di lokasi pintu/objek kunci. |
| **Template Reference** | ID referensi slot penyimpanan sidik jari pada memori sensor lokal, bukan data gambar biometrik mentah. |
| **Local-First / Edge Autonomy** | Kemampuan perangkat untuk mengambil keputusan otentikasi dan membuka kunci secara lokal tanpa bergantung pada koneksi jaringan/server. |

---

## 2. Product Overview

### 2.1 System Description

Sistem Smart Key adalah solusi akses kontrol terdistribusi dengan arsitektur **Decoupled Edge–Backend** yang menggabungkan keamanan biometrik di tingkat perangkat (edge) dengan fleksibilitas pemantauan berbasis web. Perangkat keras mengambil keputusan buka/tutup secara mandiri, sementara backend mencatat riwayat aktivitas dan mengakomodasi manajemen administratif.

### 2.2 Core Principles

- **Edge Autonomy:** Ketersediaan jaringan/backend bukan prasyarat operasional kunci.
- **Privacy by Design:** Server backend tidak pernah memproses atau menyimpan data biometrik mentah.
- **Multi-Device Scalability:** Kredensial dipetakan spesifik per perangkat dan slot sensor (*device-specific slot*).
- **Fail-Safe Security:** Kegagalan sistem secara default mengembalikan perangkat ke posisi terkunci (Locked).

### 2.3 System Boundaries

- **Termasuk dalam Batas Sistem:** Firmware ESP32, REST API Backend, Database Relational, Web Dashboard, dan protokol komunikasi IoT.
- **Diluar Batas Sistem:** Infrastruktur jaringan internet lokal (ISP), catu daya utama bangunan, dan perangkat fisik pintu/gerbang eksternal.

### 2.4 Assumptions & Constraints

- Perangkat ESP32 memiliki koneksi jaringan yang stabil untuk pengiriman log, namun sistem tetap berfungsi normal saat offline.
- Sensor sidik jari mendukung penyimpanan template secara internal di dalam modulnya.

---

## 3. System Architecture

### 3.1 Architecture Overview

Arsitektur sistem menggunakan pola **Decoupled Edge–Backend Architecture**. Perangkat edge bertindak mandiri dalam verifikasi akses, sementara backend dan dashboard bertindak sebagai *management & telemetry plane*.

### 3.2 Edge Device

Bertanggung jawab penuh atas pembacaan sensor biometrik, eksekusi keputusan otentikasi lokal, penggerak aktuator mekanis, dan pelaporan status/log secara asinkron.

### 3.3 Backend

Menyediakan layanan REST API untuk autentikasi admin, sinkronisasi data kredensial, penerimaan event logs, dan antrean perintah (*command queue*).

### 3.4 Database

Penyimpanan relasional terpusat untuk mengelola entitas pengguna, perangkat, kredensial, log audit, dan antrean perintah.

### 3.5 Web Dashboard

Antarmuka web responsif bagi administrator untuk memantau status sistem dan melakukan manajemen operasional.

### 3.6 Communication Flow

- **Edge ke Backend:** Menggunakan protokol HTTPS untuk pengiriman heartbeat, telemetri, dan event logs.
- **Backend ke Edge:** Menggunakan mekanisme polling / antrean terenkripsi ber-ID untuk perintah administratif dan remote unlock.

---

## 4. Hardware Requirements

### 4.1 ESP32

Mikrokontroler utama dengan dukungan Wi-Fi, memori flash minimal 4MB, serta ketersediaan pin GPIO yang memadai untuk komunikasi serial dan kontrol relay.

### 4.2 Fingerprint Sensor

Modul sensor sidik jari optik/kapasitif (AS608 / R307) yang mendukung komunikasi UART dan penyimpanan template internal.

### 4.3 Actuator

Motor Servo (untuk prototipe) atau Solenoid Door Lock 12V yang dikendalikan melalui modul Relay.

### 4.4 Indicators

- LED RGB atau Indikator LED Tunggal (Merah/Hijau/Biru) untuk status visual.
- Layar OLED I2C untuk teks status perangkat.

### 4.5 Power & Grounding

- **MCU + Sensor:** Regulated supply sesuai kebutuhan ESP32 dan sensor.
- **Actuator:** Dedicated power supply sesuai voltage/current aktuator yang dipilih.
- **Grounding:** Jalur ground harus dikonfigurasi sesuai kebutuhan rangkaian kontrol dan isolasi yang digunakan. Jika sinyal kontrol tidak menggunakan isolasi galvanis, MCU dan aktuator harus memiliki referensi ground yang kompatibel.

### 4.6 Hardware Constraints

Suhu operasional standar perangkat keras edge harus memadai untuk lingkungan penempatan fisik.

---

## 5. Firmware Requirements

### 5.1 Boot & Initialization

Saat perangkat dinyalakan atau setelah pemulihan daya (*power recovery*), firmware menjalankan alur:

`BOOT ➔ INITIALIZE ➔ HARDWARE_CHECK ➔ PASS (masuk ke state LOCKED ➔ IDLE)` atau `FAIL (masuk ke state ERROR_SAFE)`.

Perangkat tidak boleh langsung masuk ke mode operasional sebelum pemeriksaan sensor berhasil dan aktuator dipastikan dalam posisi terkunci.

### 5.2 Fingerprint Verification

Firmware mendeteksi sentuhan pada sensor, melakukan pemindaian, dan mencocokkan template dengan database internal sensor.

### 5.3 Unlock & Auto-Lock

Jika verifikasi berhasil (MATCH), firmware menggerakkan aktuator ke kondisi UNLOCKED, menyalakan indikator hijau, dan memulai timer. Setelah durasi tertentu (5 detik), firmware secara otomatis mengembalikan aktuator ke kondisi LOCKED.

### 5.4 Error Handling & Recovery

Jika terjadi kegagalan sensor atau pembacaan tidak valid, sistem memicu status aman (ERROR_SAFE), memastikan pintu tetap terkunci, dan menjalankan prosedur pemulihan (*recovery*) sebelum kembali melakukan Hardware Check.

### 5.5 Watchdog

Implementasi Hardware/Software Watchdog Timer untuk mereset ESP32 secara otomatis jika terjadi kebuntuan sistem (*hang*).

### 5.6 Local Events & Persistent Buffer

Setiap kejadian menghasilkan struktur data event lokal yang disimpan pada media penyimpanan persisten (persistent storage ESP32) agar tidak hilang saat perangkat kehilangan daya, untuk kemudian disinkronkan ke backend saat jaringan pulih.

---

## 6. State Machine

### 6.1 States

| State | Deskripsi |
|---|---|
| `BOOT` | Inisialisasi awal sistem. |
| `INITIALIZE` | Konfigurasi modul dan pin. |
| `HARDWARE_CHECK` | Pemeriksaan kesehatan sensor dan aktuator. |
| `LOCKED` | Kondisi aktuator terkunci secara fisik. |
| `IDLE` | Kondisi perangkat aman/terkunci dan siap menerima input sidik jari atau perintah backend. |
| `VERIFYING` | Proses pencocokan sidik jari. |
| `UNLOCKED` | Kondisi sementara aktuator membuka kunci. |
| `DENIED` | Akses ditolak (indikator merah). |
| `ERROR_SAFE` | Penanganan gangguan perangkat keras (pintu dipaksa terkunci). |

### 6.2 Transitions

```
BOOT ➔ INITIALIZE ➔ HARDWARE_CHECK
PASS ➔ LOCKED ➔ IDLE
FAIL ➔ ERROR_SAFE
IDLE + Finger Detected ➔ VERIFYING
VERIFYING + Match ➔ UNLOCKED ➔ Timeout ➔ LOCKED ➔ IDLE
VERIFYING + No Match ➔ DENIED ➔ IDLE
Anomali Perangkat Keras ➔ ERROR_SAFE ➔ Recovery ➔ HARDWARE_CHECK
POWER RESTORED ➔ BOOT ➔ INITIALIZE ➔ HARDWARE_CHECK
```

### 6.3 Failure States

Selama berada dalam state `ERROR_SAFE`, perintah pembukaan kunci dilarang keras, dan aktuator dipaksa tertutup secara mekanis sampai proses recovery berhasil.

---

## 7. User & Access Management

### 7.1 Users

Entitas individu yang memiliki hak akses sistem, dikelola melalui nama dan status akun (active/revoked).

### 7.2 Fingerprint Credentials

Kredensial biometrik yang terikat pada pasangan `user_id`, `device_id`, dan `sensor_slot_id` tertentu.

### 7.3 Enrollment Lifecycle

Alur pendaftaran sidik jari dikelola melalui status:

`PENDING ➔ WAITING_FOR_SCAN ➔ PROCESSING ➔ COMPLETED`

dengan opsi kegagalan seperti `FAILED`, `EXPIRED`, atau `CANCELLED`.

### 7.4 Revocation (Local-First Compliance)

Pencabutan kredensial menandai kredensial sebagai `revoked` pada backend dan mengirimkan perubahan tersebut ke perangkat terkait. Perangkat menerapkan pencabutan slot template setelah menerima dan berhasil memproses sinkronisasi tersebut.

---

## 8. Backend Requirements

| # | Modul | Deskripsi |
|---|---|---|
| 8.1 | **Authentication** | Menyediakan mekanisme login berbasis token/sesi yang aman khusus untuk administrator. |
| 8.2 | **Device Management** | Pencatatan status perangkat (online/offline, versi firmware, waktu last seen). |
| 8.3 | **Access Logs** | Penerimaan, penyimpanan, dan penyaringan log riwayat akses dari seluruh perangkat terdaftar. |
| 8.4 | **Commands** | Manajemen antrean perintah asinkron ber-ID unik dengan validasi masa kedaluwarsa dan idempotensi untuk instruksi jarak jauh ke perangkat edge. |
| 8.5 | **Enrollment Workflow** | Pengelolaan status alur pendaftaran sidik jari secara terstruktur antara dasbor admin dan perangkat keras. |

---

## 9. Database Requirements

### 9.1 `users`

| Kolom | Tipe |
|---|---|
| id | PK, UUID/Integer |
| name | String |
| status | Enum: active, revoked |
| created_at | Timestamp |

### 9.2 `fingerprint_credentials`

| Kolom | Tipe |
|---|---|
| id | PK |
| user_id | FK to users |
| device_id | FK to devices |
| sensor_slot_id | Integer |
| status | Enum: active, revoked |
| enrolled_at | Timestamp |
| revoked_at | Timestamp, nullable |

### 9.3 `devices`

| Kolom | Tipe |
|---|---|
| id | PK, String/Device Identifier e.g., DOOR-01 |
| name | String |
| location | String |
| status | Enum: online, offline |
| firmware_version | String |
| last_seen | Timestamp |

### 9.4 `access_logs`

| Kolom | Tipe |
|---|---|
| id | PK |
| timestamp | Timestamp |
| device_id | FK to devices |
| user_id | FK to users, nullable |
| fingerprint_slot | Integer, nullable |
| command_id | FK to commands, nullable |
| result | Enum: GRANTED, DENIED |
| reason | String |
| source | Enum: biometric, remote_command |

### 9.5 `commands`

| Kolom | Tipe |
|---|---|
| id | PK, UUID |
| device_id | FK to devices |
| command_type | String e.g., REMOTE_UNLOCK, ENROLL |
| payload | JSON |
| status | Enum: pending, acknowledged, executed, expired, failed, cancelled |
| issued_at | Timestamp |
| expires_at | Timestamp |
| created_at | Timestamp |

---

## 10. API Requirements

| # | Endpoint | Deskripsi |
|---|---|---|
| 10.1 | **Device Authentication** | Endpoint bagi ESP32 untuk melakukan otentikasi awal dan mendapatkan token akses unik. |
| 10.2 | **Events** | Endpoint untuk menerima pengiriman log kejadian dari buffer persisten perangkat edge. |
| 10.3 | **Device Heartbeat** | Endpoint periodik untuk memperbarui status online dan kesehatan perangkat (last_seen). |
| 10.4 | **Enrollment** | Endpoint untuk menginisialisasi dan menyinkronkan status siklus hidup pendaftaran kredensial baru. |
| 10.5 | **Remote Unlock** | Endpoint administratif untuk mengirimkan perintah buka kunci jarak jauh dengan menyertakan `command_id`, `issued_at`, dan `expires_at`. |
| 10.6 | **Acknowledgement** | Endpoint bagi perangkat untuk melaporkan status eksekusi perintah kembali ke backend. |

---

## 11. Web Dashboard Requirements

| # | Halaman | Deskripsi |
|---|---|---|
| 11.1 | **Admin Login** | Halaman otentikasi aman untuk administrator sistem. |
| 11.2 | **Dashboard** | Tampilan ringkasan metrik utama: total perangkat online, total akses hari ini, dan grafik aktivitas. |
| 11.3 | **Users** | Manajemen data daftar pengguna (tambah, ubah, nonaktifkan). |
| 11.4 | **Devices** | Manajemen daftar perangkat keras terdaftar dan status konektivitasnya. |
| 11.5 | **Enrollment** | Antarmuka untuk memicu dan memantau siklus pendaftaran sidik jari baru ke perangkat target. |
| 11.6 | **Access Logs** | Tabel audit trail lengkap dengan filter berdasarkan rentang tanggal, status, sumber, dan perangkat. |
| 11.7 | **Remote Unlock** | Fitur pengiriman perintah buka kunci jarak jauh dengan validasi keamanan ketat dan pelacakan status command. |

---

## 12. Security Requirements

| # | Aspek | Deskripsi |
|---|---|---|
| 12.1 | **Authentication** | Password administrator wajib di-hash menggunakan algoritma aman (Bcrypt / Argon2). |
| 12.2 | **Authorization** | Pemisahan hak akses berbasis peran administrator. |
| 12.3 | **Device Identity** | Setiap perangkat ESP32 memiliki token identitas unik untuk mencegah pemalsuan perangkat. |
| 12.4 | **Token Security** | Token API perangkat dan sesi web dikirimkan melalui jalur aman (HTTPS). |
| 12.5 | **Replay Protection & Idempotency** | Perintah jarak jauh wajib menyertakan `command_id`, `issued_at`, dan `expires_at`. `command_id` harus unik untuk setiap perintah dan diproses secara idempotent; command yang sama tidak boleh dieksekusi lebih dari satu kali. ESP32 wajib memverifikasi masa kedaluwarsa serta menolak `command_id` yang sudah pernah dieksekusi sebelumnya. |
| 12.6 | **Biometric Privacy** | Larangan keras transmisi atau penyimpanan citra sidik jari mentah di server backend. |
| 12.7 | **Audit Trail** | Seluruh tindakan administratif, upaya akses biometrik, dan perintah jarak jauh dicatat dalam sistem dan harus dipertahankan sebagai riwayat sistem serta tidak boleh dihapus melalui operasi administratif normal. |

---

## 13. Reliability & Failure Handling

| # | Skenario | Penanganan |
|---|---|---|
| 13.1 | **Offline Mode** | Perangkat tetap menjalankan fungsi verifikasi sidik jari secara lokal meskipun jaringan terputus. |
| 13.2 | **Network Failure** | Log akses yang belum terkirim disimpan pada persistent storage perangkat dan diunggah ulang saat koneksi pulih. |
| 13.3 | **Sensor Failure** | Jika modul sensor gagal merespons, firmware memicu status ERROR_SAFE (terkunci) dan mencatat kesalahan sistem. |
| 13.4 | **Actuator Failure** | Mekanisme penguncian fisik dirancang secara mekanis agar tetap tertutup secara default saat kehilangan daya. |
| 13.5 | **Power Loss & Recovery** | Saat terjadi pemadaman listrik, perangkat kehilangan daya dan posisi mekanis terkunci. Saat daya pulih, sistem menjalankan siklus: `POWER RESTORED ➔ BOOT ➔ INITIALIZE ➔ HARDWARE_CHECK ➔ (PASS ➔ LOCKED ➔ IDLE / FAIL ➔ ERROR_SAFE)`. |
| 13.6 | **Backend Failure** | Kegagalan server backend tidak mengganggu fungsi utama pembukaan pintu oleh pengguna lokal. |
| 13.7 | **Recovery** | Penerapan Watchdog Timer otomatis untuk memulihkan perangkat dari kondisi anomali. |

---

## 14. Logging & Observability

- Perangkat edge mencatat log operasional secara internal dan menyimpannya ke persistent buffer.
- Backend mencatat log HTTP request, error sistem, dan audit aktivitas admin.
- Dashboard menampilkan status telemetri perangkat secara real-time.

---

## 15. Functional Requirements (FR)

| ID | Requirement |
|---|---|
| FR-001 | Sistem harus dapat memverifikasi sidik jari secara lokal dalam waktu kurang dari 2 detik. |
| FR-002 | Sistem harus mendukung pendaftaran kredensial secara lokal di perangkat. |
| FR-003 | Sistem harus menolak sidik jari yang tidak dikenal (*unknown fingerprint rejection*). |
| FR-004 | Sistem harus membuka kunci aktuator saat terdeteksi match yang valid. |
| FR-005 | Sistem harus mengunci kembali pintu secara otomatis setelah durasi timeout tercapai. |
| FR-006 | Sistem harus dapat beroperasi secara mandiri saat offline. |
| FR-007 | Perangkat harus menghasilkan struktur data event lokal setiap kali terjadi upaya akses. |
| FR-008 | Perangkat harus menyangga event akses pada persistent storage lokal. |
| FR-009 | Perangkat harus menyinkronkan event tersimpan ke backend saat jaringan terhubung. |
| FR-010 | Sistem harus menyediakan manajemen data pengguna pada dasbor web. |
| FR-011 | Sistem harus menyediakan manajemen status perangkat keras. |
| FR-012 | Sistem harus mendukung pemetaan kredensial biometrik per perangkat dan slot sensor. |
| FR-013 | Sistem harus mendukung pencabutan kredensial (*revocation*) dengan mekanisme sinkronisasi lokal. |
| FR-014 | Sistem harus mencatat dan menampilkan log riwayat akses. |
| FR-015 | Perangkat harus mengirimkan sinyal heartbeat secara periodik. |
| FR-016 | Sistem harus menyediakan alur siklus hidup enrollment yang terstruktur. |
| FR-017 | Sistem harus mendukung eksekusi remote unlock dari dasbor admin secara idempotent. |
| FR-018 | Perangkat harus mengirimkan acknowledgement status eksekusi perintah ke backend. |
| FR-019 | Perangkat harus menolak perintah yang telah melewati batas kedaluwarsa (*command expiration*). |
| FR-020 | Sistem harus mencatat seluruh audit trail administratif dan mempertahankannya sebagai riwayat sistem yang tidak dapat dihapus melalui operasi administratif normal. |

---

## 16. Non-Functional Requirements (NFR)

| ID | Kategori | Requirement |
|---|---|---|
| NFR-001 | Security | Seluruh komunikasi jaringan antara perangkat dan backend wajib menggunakan enkripsi Transport Layer Security (TLS). |
| NFR-002 | Availability | Fungsi verifikasi akses lokal tidak boleh bergantung pada koneksi jaringan atau ketersediaan backend. Selama perangkat memiliki daya dan komponen keamanan utama berfungsi, proses autentikasi lokal tetap dapat dilakukan secara mandiri. |
| NFR-003 | Scalability | Arsitektur database dan backend harus mampu mendukung ekspansi penambahan perangkat keras hingga puluhan unit (*multi-device fleet*). |

---

## 17. Testing & Validation

| # | Jenis Pengujian | Deskripsi |
|---|---|---|
| 17.1 | **Unit Testing** | Pengujian fungsi logika individual pada modul backend dan pustaka perangkat lunak. |
| 17.2 | **Firmware Testing** | Pengujian state machine firmware menggunakan lingkungan simulasi atau pengujian bangku (*bench testing*). |
| 17.3 | **Integration Testing** | Pengujian alur komunikasi antara ESP32, REST API Backend, dan Database. |
| 17.4 | **Hardware Testing** | Pengujian respons sensor sidik jari, kestabilan motor servo/relay, dan suplai catu daya terpisah. |
| 17.5 | **Security Testing** | Pengujian ketahanan terhadap upaya manipulasi token, replay attack (validasi command_id dan idempotensi), serta akses tidak sah pada API. |
| 17.6 | **Failure Testing** | Pengujian simulasi pemutusan jaringan (offline test), pemadaman listrik mendadak (power-cut & recovery test), dan simulasi sensor error (ERROR_SAFE). |

---

## 18. Simulation Requirements

Pada tahap awal pengembangan, perangkat dapat disimulasikan menggunakan simulator perangkat keras virtual (seperti Wokwi) atau pengganti sensor fisik menggunakan push button dan serial trigger.

---

## 19. Deployment Requirements

- Firmware ESP32 diunggah menggunakan compiler standar (Arduino IDE / ESP-IDF).
- Backend dan Database dideploy pada lingkungan server kontainerisasi (Docker) atau cloud instance standar.
- Dashboard web di-host pada platform hosting yang mendukung koneksi HTTPS.

---

## 20. Implementation Phases (Execution Order)

| Fase | Nama | Deskripsi |
|---|---|---|
| **1** | Hardware Prototype | Implementasi lokal ESP32 + Sensor Sidik Jari + Servo/Relay + State Machine (Tanpa backend). |
| **2** | Local Access Control | Validasi logika verifikasi, Hardware Check, penanganan error safe, dan persistent event buffer. |
| **3** | Backend & Device Communication | Integrasi komunikasi jaringan, autentikasi perangkat, dan sinkronisasi event. |
| **4** | Web Dashboard | Pembangunan antarmuka web untuk log audit dan status perangkat. |
| **5** | Enrollment & Credential Management | Implementasi siklus hidup pendaftaran sidik jari jarak jauh dari dasbor web dan sinkronisasi revocation. |
| **6** | Remote Unlock | Penerapan kontrol buka kunci jarak jauh dengan proteksi replay, validasi kedaluwarsa, dan eksekusi idempotent. |
| **7** | Security, Testing & Final Validation | Penguatan keamanan (TLS, otentikasi ketat, pengujian kegagalan, dan validasi akhir). |

---

## 21. Acceptance Criteria

- Perangkat mampu memverifikasi sidik jari dan membuka kunci secara lokal saat sidik jari valid dikenali tanpa koneksi internet.
- Log akses berhasil disimpan dalam persistent buffer, dikirim, dan ditampilkan di dasbor web saat perangkat terhubung kembali ke jaringan.
- Fitur remote unlock dari dasbor berhasil menggerakkan aktuator perangkat setelah melalui validasi keamanan command_id, masa kedaluwarsa, dan perilaku idempotent.

---

## 22. Definition of Done

- Kode program telah melalui code review dan pengujian unit/integrasi.
- Dokumentasi API dan panduan instalasi perangkat keras telah tersedia lengkap.
- Fitur lulus seluruh kriteria penerimaan (Acceptance Criteria) pada lingkungan pengujian akhir.