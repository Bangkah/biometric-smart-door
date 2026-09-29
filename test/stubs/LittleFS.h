#pragma once
/*
 * test/stubs/LittleFS.h — filesystem LittleFS palsu TAPI FUNGSIONAL
 * (in-memory), untuk menguji ring buffer event_logger secara nyata
 * (bukan hanya syntax-check): mendukung open "w"/"r"/"r+", seek,
 * read, write, size, dan tetap PERSISTEN antar objek EventLogger
 * dalam satu proses test (mensimulasikan flash yang selamat dari
 * "reboot" = instance EventLogger baru dibuat, isi file tetap ada).
 */
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <vector>
#include <map>

// Backing store global: path -> isi byte. Disengaja global (bukan anggota
// LittleFSFS) supaya bertahan lintas instance EventLogger, persis seperti
// flash fisik yang tidak ikut hilang saat objek firmware di-reset.
std::map<std::string, std::vector<uint8_t>>& __test_fake_fs();

// Dipanggil dari setUp() tiap test case untuk mengosongkan "flash".
void __test_reset_fake_fs();

class File {
  public:
    File() : _data(nullptr), _pos(0), _valid(false) {}
    explicit File(std::vector<uint8_t>* data) : _data(data), _pos(0), _valid(data != nullptr) {}

    operator bool() const { return _valid; }
    size_t size() const { return _data ? _data->size() : 0; }
    void close() { _valid = false; _data = nullptr; }

    bool seek(uint32_t pos) {
      if (!_valid || !_data || pos > _data->size()) return false;
      _pos = pos;
      return true;
    }

    size_t readBytes(char* buf, size_t len) {
      if (!_valid || !_data) return 0;
      size_t avail = _data->size() - _pos;
      size_t n = (len < avail) ? len : avail;
      if (n > 0) memcpy(buf, _data->data() + _pos, n);
      _pos += n;
      return n;
    }

    size_t write(const uint8_t* buf, size_t len) {
      if (!_valid || !_data) return 0;
      if (_pos + len > _data->size()) _data->resize(_pos + len);
      memcpy(_data->data() + _pos, buf, len);
      _pos += len;
      return len;
    }

    void flush() { /* no-op: penulisan langsung ke backing store */ }

  private:
    std::vector<uint8_t>* _data;
    size_t _pos;
    bool _valid;
};

class LittleFSFS {
  public:
    bool begin(bool formatOnFail = false) { (void)formatOnFail; return true; }

    bool exists(const char* path) {
      return __test_fake_fs().count(path) > 0;
    }

    File open(const char* path, const char* mode = "r") {
      auto& fs = __test_fake_fs();
      std::string p(path);
      std::string m(mode);

      if (m == "w") {
        fs[p] = std::vector<uint8_t>();  // buat/truncate
        return File(&fs[p]);
      }
      // "r" atau "r+": harus sudah ada (persis semantik LittleFS asli)
      auto it = fs.find(p);
      if (it == fs.end()) return File();  // invalid -> operator bool() == false
      return File(&it->second);
    }
};
extern LittleFSFS LittleFS;
