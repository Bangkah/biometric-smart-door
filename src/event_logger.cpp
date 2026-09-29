#include "event_logger.h"
#include "config.h"

#include <LittleFS.h>
#include <Preferences.h>
#include <string.h>
#include <time.h>

EventLogger eventLogger;

const char* eventTypeToString(EventType type) {
  switch (type) {
    case EventType::SYSTEM_BOOT:          return "SYSTEM_BOOT";
    case EventType::HARDWARE_CHECK_FAIL:  return "HARDWARE_CHECK_FAIL";
    case EventType::ACCESS_GRANTED:       return "ACCESS_GRANTED";
    case EventType::ACCESS_DENIED:        return "ACCESS_DENIED";
    case EventType::ERROR_SAFE_TRIGGERED: return "ERROR_SAFE_TRIGGERED";
    case EventType::RECOVERY_ATTEMPT:     return "RECOVERY_ATTEMPT";
    case EventType::LOG_CLEARED:          return "LOG_CLEARED";
    default:                              return "UNKNOWN";
  }
}

static const size_t RECORD_SIZE = sizeof(EventRecord);
static const size_t FILE_SIZE   = RECORD_SIZE * EVENT_LOG_MAX_ENTRIES;

namespace {

// RAII untuk mutex ring buffer (dilepas otomatis di semua jalur return).
class LockGuard {
  public:
    LockGuard(SemaphoreHandle_t m, uint32_t timeoutMs) : _m(m), _ok(false) {
      if (_m != nullptr) _ok = (xSemaphoreTake(_m, pdMS_TO_TICKS(timeoutMs)) == pdTRUE);
    }
    ~LockGuard() { if (_ok) xSemaphoreGive(_m); }
    bool ok() const { return _ok; }
  private:
    LockGuard(const LockGuard&);
    LockGuard& operator=(const LockGuard&);
    SemaphoreHandle_t _m;
    bool _ok;
};

// Field struct packed tidak boleh di-bind ke reference -> salin ke variabel lokal.
void formatLocalTime(uint32_t unixTs, char* buf, size_t len) {
  if (unixTs == 0) { snprintf(buf, len, "(waktu n/a)"); return; }
  time_t t = static_cast<time_t>(unixTs);
  struct tm tmInfo;
  localtime_r(&t, &tmInfo);
  strftime(buf, len, "%Y-%m-%d %H:%M:%S", &tmInfo);
}

void printEventLine(const EventRecord &rec) {
  uint32_t seq = rec.sequence;
  uint16_t boot = rec.bootId;
  uint32_t ms = rec.timestampMs;
  uint32_t unixTs = rec.unixTimestamp;
  int16_t detail = rec.detail;
  char ts[24];
  formatLocalTime(unixTs, ts, sizeof(ts));

  Serial.printf("[EVENT #%lu] boot=%u t+%lums time=%s type=%s detail=%d msg=\"%s\"\n",
                (unsigned long)seq, (unsigned)boot, (unsigned long)ms, ts,
                eventTypeToString(static_cast<EventType>(rec.type)),
                (int)detail, rec.message);
}

}  // namespace

// ------------------------------------------------------------
// begin()
// ------------------------------------------------------------
bool EventLogger::begin() {
  if (_mutex == nullptr) _mutex = xSemaphoreCreateMutex();
  if (_mutex == nullptr) {
    Serial.println("[EVENT_LOGGER] GAGAL membuat mutex!");
    return false;
  }

  if (!LittleFS.begin(true)) {  // true = auto-format bila belum pernah diformat
    Serial.println("[EVENT_LOGGER] GAGAL mount LittleFS!");
    _ready = false;
    return false;
  }

  // bootId & epoch di NVS: ditulis jarang (sekali per boot / per hapus-log),
  // TERPISAH dari ring buffer sehingga tidak menambah wear pada partisi log.
  Preferences prefs;
  prefs.begin(EVENT_LOG_NVS_NAMESPACE, false);
  uint32_t bootCounter = prefs.getUInt("bootCount", 0) + 1;
  prefs.putUInt("bootCount", bootCounter);
  _epoch = prefs.getUInt("epoch", 0);
  prefs.end();
  _bootId = static_cast<uint16_t>(bootCounter % 65536);

  bool initialized = false;
  if (!ensureStorageFile(initialized)) {
    Serial.println("[EVENT_LOGGER] GAGAL menyiapkan file ring buffer!");
    _ready = false;
    return false;
  }
  if (initialized) bumpEpoch();  // log baru = generasi baru

  recoverStateFromFlash();
  _ready = true;

  Serial.printf("[EVENT_LOGGER] Siap. bootId=%u, epoch=%lu, entri tersimpan=%lu/%d\n",
                (unsigned)_bootId, (unsigned long)_epoch, (unsigned long)_count,
                EVENT_LOG_MAX_ENTRIES);
  return true;
}

void EventLogger::bumpEpoch() {
  Preferences prefs;
  prefs.begin(EVENT_LOG_NVS_NAMESPACE, false);
  _epoch = prefs.getUInt("epoch", 0) + 1;
  prefs.putUInt("epoch", _epoch);
  prefs.end();
}

// ------------------------------------------------------------
// ensureStorageFile()
// Membuat file berukuran tetap jika belum ada / ukurannya tidak cocok
// (mis. EVENT_LOG_MAX_ENTRIES atau layout EventRecord berubah).
// Slot baru diisi nol -> sequence == 0 == "kosong".
// ------------------------------------------------------------
bool EventLogger::ensureStorageFile(bool &initialized) {
  initialized = false;
  bool needsInit = true;

  if (LittleFS.exists(EVENT_LOG_FILE)) {
    File f = LittleFS.open(EVENT_LOG_FILE, "r");
    if (f) {
      if (f.size() == FILE_SIZE) {
        needsInit = false;
      } else {
        Serial.printf("[EVENT_LOGGER] PERINGATAN: ukuran file log (%u B) != yang diharapkan (%u B). "
                      "Log lama dibuang & dibuat ulang (mis. upgrade dari firmware dengan layout record berbeda).\n",
                      (unsigned)f.size(), (unsigned)FILE_SIZE);
      }
      f.close();
    }
  }

  if (!needsInit) return true;

  Serial.println("[EVENT_LOGGER] Menginisialisasi file ring buffer baru...");
  File f = LittleFS.open(EVENT_LOG_FILE, "w");  // "w" = buat/truncate
  if (!f) return false;

  EventRecord empty;
  memset(&empty, 0, sizeof(empty));
  for (uint32_t i = 0; i < EVENT_LOG_MAX_ENTRIES; i++) {
    f.write(reinterpret_cast<const uint8_t*>(&empty), RECORD_SIZE);
  }
  f.close();
  initialized = true;
  return true;
}

// ------------------------------------------------------------
// readRecord() / writeRecord()  (pemanggil WAJIB memegang lock,
// kecuali saat begin() yang belum multi-task)
// ------------------------------------------------------------
bool EventLogger::readRecord(uint32_t slot, EventRecord &out) const {
  if (slot >= EVENT_LOG_MAX_ENTRIES) return false;

  File f = LittleFS.open(EVENT_LOG_FILE, "r");
  if (!f) return false;
  if (!f.seek(slot * RECORD_SIZE)) { f.close(); return false; }

  size_t n = f.readBytes(reinterpret_cast<char*>(&out), RECORD_SIZE);
  f.close();
  return (n == RECORD_SIZE);
}

bool EventLogger::writeRecord(uint32_t slot, const EventRecord &rec) {
  if (slot >= EVENT_LOG_MAX_ENTRIES) return false;

  // "r+" (bukan "w") agar TIDAK men-truncate seluruh file.
  File f = LittleFS.open(EVENT_LOG_FILE, "r+");
  if (!f) return false;
  if (!f.seek(slot * RECORD_SIZE)) { f.close(); return false; }

  size_t written = f.write(reinterpret_cast<const uint8_t*>(&rec), RECORD_SIZE);
  f.flush();  // paksa ke flash sebelum lanjut (ketahanan power-loss)
  f.close();
  return (written == RECORD_SIZE);
}

// ------------------------------------------------------------
// recoverStateFromFlash()
// Posisi tulis berikutnya = slot dengan sequence tertinggi + 1.
// Tidak bergantung pada metadata terpisah -> tahan power-loss.
// ------------------------------------------------------------
void EventLogger::recoverStateFromFlash() {
  uint32_t maxSeq = 0;
  int32_t maxSeqSlot = -1;
  uint32_t validCount = 0;

  EventRecord rec;
  for (uint32_t i = 0; i < EVENT_LOG_MAX_ENTRIES; i++) {
    if (!readRecord(i, rec)) continue;
    uint32_t seq = rec.sequence;
    if (seq == 0) continue;

    validCount++;
    if (seq > maxSeq) {
      maxSeq = seq;
      maxSeqSlot = static_cast<int32_t>(i);
    }
  }

  if (maxSeqSlot < 0) {
    _writeIndex = 0;
    _count = 0;
    _nextSequence = 1;
  } else {
    _writeIndex = (static_cast<uint32_t>(maxSeqSlot) + 1) % EVENT_LOG_MAX_ENTRIES;
    _nextSequence = maxSeq + 1;
    _count = validCount;
  }
}

// ------------------------------------------------------------
// appendLocked(): pemanggil memegang lock. rec selalu terisi
// (juga saat gagal) agar bisa dicetak.
// ------------------------------------------------------------
bool EventLogger::appendLocked(EventType type, int16_t detail, const char* message, EventRecord &rec) {
  memset(&rec, 0, sizeof(rec));
  rec.sequence      = _nextSequence;
  rec.timestampMs   = millis();
  rec.unixTimestamp = _timeProvider ? _timeProvider() : 0;  // 0 jika belum sync
  rec.bootId        = _bootId;
  rec.type          = static_cast<uint8_t>(type);
  rec.detail        = detail;
  strncpy(rec.message, message ? message : "", sizeof(rec.message) - 1);
  rec.message[sizeof(rec.message) - 1] = '\0';

  if (!writeRecord(_writeIndex, rec)) return false;

  _nextSequence++;
  _writeIndex = (_writeIndex + 1) % EVENT_LOG_MAX_ENTRIES;
  if (_count < EVENT_LOG_MAX_ENTRIES) _count++;
  return true;
}

// ------------------------------------------------------------
// logEvent()
// ------------------------------------------------------------
void EventLogger::logEvent(EventType type, int16_t detail, const char* message) {
  if (!_ready) {
    Serial.println("[EVENT_LOGGER] belum siap, event dilewati.");
    return;
  }

  EventRecord rec;
  bool ok = false;
  {
    LockGuard guard(_mutex, EVENT_LOG_LOCK_TIMEOUT_MS);
    if (!guard.ok()) {
      Serial.println("[EVENT_LOGGER] PERINGATAN: gagal mengambil lock, event dilewati.");
      return;
    }
    ok = appendLocked(type, detail, message, rec);
  }  // lock dilepas sebelum mencetak ke Serial (menjaga masa tahan lock singkat)

  if (!ok) Serial.println("[EVENT_LOGGER] GAGAL menulis event ke flash!");
  printEventLine(rec);
}

// ------------------------------------------------------------
// dumpToSerial()  — terlama -> terbaru
// ------------------------------------------------------------
void EventLogger::dumpToSerial() const {
  LockGuard guard(_mutex, EVENT_LOG_LOCK_TIMEOUT_MS);
  if (!guard.ok()) {
    Serial.println("[EVENT_LOGGER] gagal mengambil lock untuk dump.");
    return;
  }

  Serial.println("========== EVENT LOG (terlama -> terbaru) ==========");
  if (_count == 0) {
    Serial.println("(kosong)");
    Serial.println("=====================================================");
    return;
  }

  // Belum wrap: entri di slot 0.._count-1. Sudah penuh/wrap: yang terlama
  // berada di _writeIndex (slot berikutnya yang akan ditimpa).
  uint32_t start = (_count < EVENT_LOG_MAX_ENTRIES) ? 0 : _writeIndex;

  for (uint32_t i = 0; i < _count; i++) {
    uint32_t slot = (start + i) % EVENT_LOG_MAX_ENTRIES;
    EventRecord rec;
    if (!readRecord(slot, rec)) continue;
    uint32_t seq = rec.sequence;
    if (seq == 0) continue;

    uint16_t boot = rec.bootId;
    uint32_t ms = rec.timestampMs;
    uint32_t unixTs = rec.unixTimestamp;
    int16_t detail = rec.detail;
    char ts[24];
    formatLocalTime(unixTs, ts, sizeof(ts));

    Serial.printf("#%lu\tboot=%u\tt+%lums\t%s\t%s\tdetail=%d\t\"%s\"\n",
                  (unsigned long)seq, (unsigned)boot, (unsigned long)ms, ts,
                  eventTypeToString(static_cast<EventType>(rec.type)),
                  (int)detail, rec.message);
  }

  Serial.printf("Total: %lu/%d entri (epoch=%lu).\n",
                (unsigned long)_count, EVENT_LOG_MAX_ENTRIES, (unsigned long)_epoch);
  Serial.println("=====================================================");
}

// ------------------------------------------------------------
// clearAll()
// ------------------------------------------------------------
void EventLogger::clearAll() {
  if (!_ready) return;
  Serial.println("[EVENT_LOGGER] Menghapus seluruh log...");

  EventRecord rec;
  bool ok = false;
  {
    LockGuard guard(_mutex, EVENT_LOG_LOCK_TIMEOUT_MS);
    if (!guard.ok()) {
      Serial.println("[EVENT_LOGGER] gagal mengambil lock untuk clear.");
      return;
    }

    EventRecord empty;
    memset(&empty, 0, sizeof(empty));
    for (uint32_t i = 0; i < EVENT_LOG_MAX_ENTRIES; i++) writeRecord(i, empty);

    _writeIndex = 0;
    _count = 0;
    _nextSequence = 1;
    bumpEpoch();  // sequence mulai dari 1 lagi -> generasi baru agar sync tidak salah lewat

    // Catat aksi penghapusan sebagai entri pertama (jejak audit, SRS 12.7).
    ok = appendLocked(EventType::LOG_CLEARED, -1, "Log dihapus via debugger", rec);
  }

  Serial.printf("[EVENT_LOGGER] Log berhasil dihapus (epoch baru=%lu).\n", (unsigned long)_epoch);
  if (!ok) Serial.println("[EVENT_LOGGER] GAGAL menulis event LOG_CLEARED!");
  printEventLine(rec);
}

// ------------------------------------------------------------
// snapshot() / collectAfter()   [Phase 3]
// ------------------------------------------------------------
bool EventLogger::snapshot(uint32_t &newestSeq, uint32_t &count, uint32_t &epoch) const {
  LockGuard guard(_mutex, EVENT_LOG_LOCK_TIMEOUT_MS);
  if (!guard.ok()) return false;
  newestSeq = _nextSequence - 1;  // 0 bila kosong
  count = _count;
  epoch = _epoch;
  return true;
}

uint32_t EventLogger::collectAfter(uint32_t afterSeq, EventRecord* out, uint32_t maxOut) const {
  if (!_ready || out == nullptr || maxOut == 0) return 0;

  LockGuard guard(_mutex, EVENT_LOG_LOCK_TIMEOUT_MS);
  if (!guard.ok() || _count == 0) return 0;

  File f = LittleFS.open(EVENT_LOG_FILE, "r");  // satu kali open untuk seluruh batch
  if (!f) return 0;

  uint32_t start = (_count < EVENT_LOG_MAX_ENTRIES) ? 0 : _writeIndex;
  uint32_t n = 0;

  for (uint32_t i = 0; i < _count && n < maxOut; i++) {
    uint32_t slot = (start + i) % EVENT_LOG_MAX_ENTRIES;
    if (!f.seek(slot * RECORD_SIZE)) break;

    EventRecord rec;
    if (f.readBytes(reinterpret_cast<char*>(&rec), RECORD_SIZE) != RECORD_SIZE) break;

    uint32_t seq = rec.sequence;
    if (seq == 0 || seq <= afterSeq) continue;
    out[n++] = rec;
  }

  f.close();
  return n;
}
