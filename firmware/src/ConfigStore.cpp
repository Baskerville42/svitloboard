#include "ConfigStore.h"

#include <LittleFS.h>
#include <cstddef>
#include <cstring>

#include "BuildConfig.h"

namespace svb {
namespace {

constexpr uint32_t kRecordMagic = 0x32564253UL;  // "SVB2" in little endian.
constexpr uint32_t kBootRecordMagic = 0x42564253UL;  // "SVBB" in little endian.
constexpr char kSlotPaths[2][10] = {"/cfg0.bin", "/cfg1.bin"};
constexpr char kTemporaryPath[] = "/cfg.tmp";
constexpr char kBootSlotPaths[2][11] = {"/boot0.bin", "/boot1.bin"};
constexpr char kBootTemporaryPath[] = "/boot.tmp";

}  // namespace

bool ConfigStore::begin() {
#if defined(ESP8266)
  mounted_ = LittleFS.begin();
  if (!mounted_) {
    diagnostics_.add(EventLevel::Warning, F("Config filesystem is unformatted; formatting once"));
    mounted_ = LittleFS.format() && LittleFS.begin();
  }
#else
  mounted_ = LittleFS.begin(true);
#endif
  if (!mounted_) diagnostics_.add(EventLevel::Error, F("Cannot mount config filesystem"));
  return mounted_;
}

bool ConfigStore::load(AppConfig& config) {
  if (!mounted_) {
    setConfigDefaults(config);
    return false;
  }

  Record records[2]{};
  const bool valid0 = readRecord(kSlotPaths[0], records[0]) && validRecord(records[0]);
  const bool valid1 = readRecord(kSlotPaths[1], records[1]) && validRecord(records[1]);

  if (!valid0 && !valid1) {
    setConfigDefaults(config);
    activeSlot_ = -1;
    generation_ = 0;
    diagnostics_.add(EventLevel::Info, F("No valid v2 config; first setup required"));
    return false;
  }

  activeSlot_ = valid0 && (!valid1 || generationAfter(records[0].generation, records[1].generation)) ? 0 : 1;
  const Record& selected = records[activeSlot_];
  config = selected.config;
  generation_ = selected.generation;
  diagnostics_.add(EventLevel::Info, String(F("Loaded config slot ")) + activeSlot_ +
                                          F(" generation ") + generation_);
  return true;
}

bool ConfigStore::save(const AppConfig& config) {
  if (!mounted_ || !isConfigSane(config)) {
    diagnostics_.add(EventLevel::Error, F("Refusing to save invalid config"));
    return false;
  }

  Record record{};
  record.magic = kRecordMagic;
  record.schema = SVB_CONFIG_SCHEMA;
  record.size = sizeof(Record);
  record.generation = generation_ + 1;
  record.config = config;
  record.crc32 = calculateCrc(reinterpret_cast<const uint8_t*>(&record), offsetof(Record, crc32));

  const int8_t targetSlot = activeSlot_ == 0 ? 1 : 0;
  LittleFS.remove(kTemporaryPath);
  if (!writeRecord(kTemporaryPath, record)) {
    diagnostics_.add(EventLevel::Error, F("Config write failed; previous slot retained"));
    return false;
  }

  Record verification{};
  if (!readRecord(kTemporaryPath, verification) || !validRecord(verification)) {
    LittleFS.remove(kTemporaryPath);
    diagnostics_.add(EventLevel::Error, F("Config read-back failed; previous slot retained"));
    return false;
  }

  LittleFS.remove(kSlotPaths[targetSlot]);
  if (!LittleFS.rename(kTemporaryPath, kSlotPaths[targetSlot])) {
    LittleFS.remove(kTemporaryPath);
    diagnostics_.add(EventLevel::Error, F("Config slot rotation failed"));
    return false;
  }

  activeSlot_ = targetSlot;
  generation_ = record.generation;
  diagnostics_.add(EventLevel::Info, String(F("Saved config generation ")) + generation_);
  return true;
}

bool ConfigStore::clear() {
  if (!mounted_) return false;
  const bool result0 = !LittleFS.exists(kSlotPaths[0]) || LittleFS.remove(kSlotPaths[0]);
  const bool result1 = !LittleFS.exists(kSlotPaths[1]) || LittleFS.remove(kSlotPaths[1]);
  LittleFS.remove(kTemporaryPath);
  const bool bootResult = discardFastBootCounter();
  activeSlot_ = -1;
  generation_ = 0;
  diagnostics_.add(EventLevel::Warning, F("Configuration erased by explicit factory reset"));
  return result0 && result1 && bootResult;
}

bool ConfigStore::recordFastBoot(uint8_t& count) {
  count = 0;
  if (!mounted_) return false;

  BootRecord records[2]{};
  const bool valid0 = readBootRecord(kBootSlotPaths[0], records[0]) &&
                      validBootRecord(records[0]);
  const bool valid1 = readBootRecord(kBootSlotPaths[1], records[1]) &&
                      validBootRecord(records[1]);
  if (!valid0 && !valid1) {
    bootActiveSlot_ = -1;
    bootGeneration_ = 0;
    bootCount_ = 0;
  } else {
    bootActiveSlot_ = valid0 &&
                              (!valid1 || generationAfter(records[0].generation,
                                                          records[1].generation))
                          ? 0
                          : 1;
    const BootRecord& selected = records[bootActiveSlot_];
    bootGeneration_ = selected.generation;
    bootCount_ = selected.count;
  }

  const uint8_t next = bootCount_ == 0xFF ? 0xFF : static_cast<uint8_t>(bootCount_ + 1);
  if (!writeBootCount(next)) return false;
  count = next;
  return true;
}

bool ConfigStore::markFastBootStable() {
  if (!mounted_) return false;
  if (bootCount_ == 0) return true;
  return writeBootCount(0);
}

bool ConfigStore::discardFastBootCounter() {
  if (!mounted_) return false;
  const bool result0 = !LittleFS.exists(kBootSlotPaths[0]) || LittleFS.remove(kBootSlotPaths[0]);
  const bool result1 = !LittleFS.exists(kBootSlotPaths[1]) || LittleFS.remove(kBootSlotPaths[1]);
  const bool temporary = !LittleFS.exists(kBootTemporaryPath) ||
                         LittleFS.remove(kBootTemporaryPath);
  bootActiveSlot_ = -1;
  bootGeneration_ = 0;
  bootCount_ = 0;
  return result0 && result1 && temporary;
}

bool ConfigStore::readRecord(const char* path, Record& record) const {
  File file = LittleFS.open(path, "r");
  if (!file || file.size() != sizeof(Record)) return false;
  const size_t bytesRead = file.read(reinterpret_cast<uint8_t*>(&record), sizeof(Record));
  file.close();
  return bytesRead == sizeof(Record);
}

bool ConfigStore::writeRecord(const char* path, const Record& record) const {
  File file = LittleFS.open(path, "w");
  if (!file) return false;
  const size_t bytesWritten = file.write(reinterpret_cast<const uint8_t*>(&record), sizeof(Record));
  file.flush();
  file.close();
  return bytesWritten == sizeof(Record);
}

bool ConfigStore::readBootRecord(const char* path, BootRecord& record) const {
  File file = LittleFS.open(path, "r");
  if (!file || file.size() != sizeof(BootRecord)) return false;
  const size_t bytesRead = file.read(reinterpret_cast<uint8_t*>(&record), sizeof(BootRecord));
  file.close();
  return bytesRead == sizeof(BootRecord);
}

bool ConfigStore::writeBootCount(uint8_t count) {
  BootRecord record{};
  record.magic = kBootRecordMagic;
  record.schema = 1;
  record.size = sizeof(BootRecord);
  record.generation = bootGeneration_ + 1;
  record.count = count;
  record.crc32 = calculateCrc(reinterpret_cast<const uint8_t*>(&record),
                              offsetof(BootRecord, crc32));

  const int8_t targetSlot = bootActiveSlot_ == 0 ? 1 : 0;
  LittleFS.remove(kBootTemporaryPath);
  File file = LittleFS.open(kBootTemporaryPath, "w");
  if (!file) return false;
  const size_t bytesWritten = file.write(reinterpret_cast<const uint8_t*>(&record),
                                         sizeof(BootRecord));
  file.flush();
  file.close();
  if (bytesWritten != sizeof(BootRecord)) {
    LittleFS.remove(kBootTemporaryPath);
    return false;
  }

  BootRecord verification{};
  if (!readBootRecord(kBootTemporaryPath, verification) || !validBootRecord(verification)) {
    LittleFS.remove(kBootTemporaryPath);
    return false;
  }
  LittleFS.remove(kBootSlotPaths[targetSlot]);
  if (!LittleFS.rename(kBootTemporaryPath, kBootSlotPaths[targetSlot])) {
    LittleFS.remove(kBootTemporaryPath);
    return false;
  }
  bootActiveSlot_ = targetSlot;
  bootGeneration_ = record.generation;
  bootCount_ = count;
  return true;
}

bool ConfigStore::validRecord(const Record& record) const {
  if (record.magic != kRecordMagic || record.schema != SVB_CONFIG_SCHEMA ||
      record.size != sizeof(Record) || !isConfigSane(record.config)) {
    return false;
  }
  return record.crc32 ==
         calculateCrc(reinterpret_cast<const uint8_t*>(&record), offsetof(Record, crc32));
}

bool ConfigStore::validBootRecord(const BootRecord& record) const {
  if (record.magic != kBootRecordMagic || record.schema != 1 ||
      record.size != sizeof(BootRecord)) {
    return false;
  }
  return record.crc32 ==
         calculateCrc(reinterpret_cast<const uint8_t*>(&record), offsetof(BootRecord, crc32));
}

uint32_t ConfigStore::calculateCrc(const uint8_t* data, size_t length) {
  uint32_t crc = 0xFFFFFFFFUL;
  for (size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc >> 1U) ^ (0xEDB88320UL & (0U - (crc & 1U)));
    }
  }
  return ~crc;
}

bool ConfigStore::generationAfter(uint32_t left, uint32_t right) {
  return static_cast<int32_t>(left - right) > 0;
}

}  // namespace svb
