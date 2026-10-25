#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

class File {
 public:
  File() = default;
  explicit File(std::vector<uint8_t>* bytes) : bytes_(bytes) {}
  explicit operator bool() const { return bytes_ != nullptr; }
  size_t size() const { return bytes_ ? bytes_->size() : 0; }
  size_t read(uint8_t* destination, size_t length) {
    if (!bytes_) return 0;
    length = std::min(length, bytes_->size());
    memcpy(destination, bytes_->data(), length);
    return length;
  }
  size_t write(const uint8_t* source, size_t length) {
    if (!bytes_) return 0;
    bytes_->assign(source, source + length);
    return length;
  }
  void flush() {}
  void close() {}
 private:
  std::vector<uint8_t>* bytes_ = nullptr;
};

class HostLittleFS {
 public:
  std::map<std::string, std::vector<uint8_t>> files;
  bool failWrites = false;
  bool failRename = false;
  bool begin() { return true; }
  bool format() { files.clear(); return true; }
  bool exists(const char* path) const { return files.count(path) != 0; }
  bool remove(const char* path) { return files.erase(path) != 0; }
  bool rename(const char* source, const char* destination) {
    if (failRename || !exists(source)) return false;
    files[destination] = files[source];
    files.erase(source);
    return true;
  }
  File open(const char* path, const char* mode) {
    if (mode[0] == 'w') {
      if (failWrites) return {};
      files[path].clear();
    } else if (!exists(path)) return {};
    return File(&files[path]);
  }
};
extern HostLittleFS LittleFS;
