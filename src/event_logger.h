#pragma once
/*
 * event_logger.h
 * ------------------------------------------------------------
 * Local Event System & Persistent Memory Buffer (SRS 5.6, 13.2,
 * FR-007, FR-008) — Phase 2, diperluas di Phase 3.
 *
 * Desain (ringkas; detail di README bagian 8 & 10):
 *  - Ring buffer berukuran tetap di LittleFS; entri terlama tertimpa
 *    saat penuh. Setiap logEvent() langsung ditulis + di-flush ke flash
 *    (tahan power-loss). Recovery saat boot memindai slot dan memakai
 *    `sequence` tertinggi (self-healing, tanpa file metadata).
 *
 *  [Phase 3] tambahan:
 *  - `unixTimestamp`: epoch UTC riil bila NTP sudah sinkron saat event
 *    dicatat (0 = belum tersedia). Sumber waktu disuntikkan lewat
 *    setTimeProvider() sehingga modul ini tidak bergantung pada Wi-Fi/NTP.
 *  - Mutex: ring buffer kini diakses dua task (loop() menulis event,
 *    network task membaca untuk upload).
 *  - `epoch` log: penghitung persisten (NVS) yang naik setiap ring
 *    buffer dibuat ulang/dihapus. `sequence` kembali ke 1 setelah itu,
 *    sehingga penanda sinkronisasi harus dipasangkan dengan epoch —
 *    tanpa ini event baru pasca-`c` tidak akan pernah terkirim.
 * ------------------------------------------------------------
 */

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

enum class EventType : uint8_t {
  SYSTEM_BOOT = 0,
  HARDWARE_CHECK_FAIL,
  ACCESS_GRANTED,
  ACCESS_DENIED,
  ERROR_SAFE_TRIGGERED,
  RECOVERY_ATTEMPT,
  LOG_CLEARED,
  UNKNOWN = 255
};

const char* eventTypeToString(EventType type);

#pragma pack(push, 1)
struct EventRecord {
  uint32_t sequence;       // ID unik naik terus; 0 = slot kosong/tidak valid
  uint32_t timestampMs;    // millis() pada sesi boot saat event dicatat
  uint32_t unixTimestamp;  // [Phase 3] epoch UTC (detik); 0 = NTP belum sinkron saat dicatat
  uint16_t bootId;         // ID sesi boot (membedakan epoch millis() antar reboot)
  uint8_t  type;           // nilai EventType
  int16_t  detail;         // kode detail opsional (mis. slot sidik jari); -1 = n/a
  char     message[28];    // pesan singkat, selalu null-terminated
};
#pragma pack(pop)

// Ukuran record ikut menentukan ukuran file ring buffer. Jika berubah,
// file lama dibuang & dibuat ulang saat boot (lihat ensureStorageFile).
static_assert(sizeof(EventRecord) == 45, "Layout EventRecord berubah: periksa migrasi log & README");

// Penyedia waktu UNIX yang disuntikkan dari luar (main.cpp), agar event_logger
// tidak perlu tahu apa pun tentang Wi-Fi/NTP. Mengembalikan 0 jika belum sinkron.
typedef uint32_t (*UnixTimeProvider)();

class EventLogger {
  public:
    // Mount LittleFS, siapkan/validasi file ring buffer, dan pulihkan posisi
    // tulis dari flash. Panggil sekali di setup() SEBELUM logEvent() pertama.
    bool begin();

    // [Phase 3] Daftarkan penyedia waktu UNIX. Boleh dipanggil sebelum begin().
    void setTimeProvider(UnixTimeProvider provider) { _timeProvider = provider; }

    // Mencatat satu event baru (thread-safe). Langsung ditulis ke flash.
    void logEvent(EventType type, int16_t detail, const char* message);

    // Jumlah entri valid saat ini (<= kapasitas). Nilai indikatif (tanpa lock).
    uint32_t count() const { return _count; }

    // Mencetak seluruh log (terlama -> terbaru). Perintah debugger 'l'.
    void dumpToSerial() const;

    // Menghapus seluruh log dan menaikkan epoch. Perintah debugger 'c'.
    void clearAll();

    // ---- [Phase 3] API untuk sinkronisasi ke backend (dipakai api_client) ----

    // ID sesi boot saat ini (untuk memperkirakan waktu event sesi berjalan).
    uint16_t bootId() const { return _bootId; }

    // Foto konsisten metadata ring buffer. newestSeq = 0 bila log kosong.
    // Mengembalikan false jika gagal mengambil lock.
    bool snapshot(uint32_t &newestSeq, uint32_t &count, uint32_t &epoch) const;

    // Menyalin hingga maxOut entri dengan sequence > afterSeq, urut
    // terlama -> terbaru. Mengembalikan jumlah entri yang disalin.
    uint32_t collectAfter(uint32_t afterSeq, EventRecord* out, uint32_t maxOut) const;

  private:
    bool _ready = false;
    uint32_t _writeIndex = 0;    // slot berikutnya yang akan ditulis
    uint32_t _count = 0;         // jumlah slot valid (maks EVENT_LOG_MAX_ENTRIES)
    uint32_t _nextSequence = 1;  // sequence untuk event berikutnya
    uint32_t _epoch = 0;         // [Phase 3] generasi log (persisten di NVS)
    uint16_t _bootId = 0;
    UnixTimeProvider _timeProvider = nullptr;
    SemaphoreHandle_t _mutex = nullptr;

    bool ensureStorageFile(bool &initialized);
    bool readRecord(uint32_t slot, EventRecord &out) const;
    bool writeRecord(uint32_t slot, const EventRecord &rec);
    void recoverStateFromFlash();
    void bumpEpoch();
    bool appendLocked(EventType type, int16_t detail, const char* message, EventRecord &outRec);
};

extern EventLogger eventLogger;
