#pragma once

#include <Arduino.h>

#include "AppConfig.h"
#include "Diagnostics.h"

namespace svb {

class ConfigStore {
 public:
  explicit ConfigStore(Diagnostics& diagnostics) : diagnostics_(diagnostics) {}

  bool begin();
  bool load(AppConfig& config);
  bool save(const AppConfig& config);
  bool clear();
  bool recordFastBoot(uint8_t& count);
  bool markFastBootStable();
  bool discardFastBootCounter();
  uint32_t generation() const { return generation_; }

 private:
#pragma pack(push, 1)
  struct LegacyAppConfigV1 {
    uint8_t setupComplete;
    uint8_t pingPaused;
    uint8_t lastWifiProfile;
    char deviceName[33];
    char channelKey[65];
    char adminPassword[33];
    uint16_t pingIntervalSeconds;
    WifiProfile wifi[kWifiProfileCount];
    ExpertConfig expert;
  };

  struct Record {
    uint32_t magic;
    uint16_t schema;
    uint16_t size;
    uint32_t generation;
    AppConfig config;
    uint32_t crc32;
  };

  struct BootRecord {
    uint32_t magic;
    uint16_t schema;
    uint16_t size;
    uint32_t generation;
    uint8_t count;
    uint8_t reserved[3];
    uint32_t crc32;
  };
#pragma pack(pop)

  bool readRecord(const char* path, Record& record) const;
  bool writeRecord(const char* path, const Record& record) const;
  bool readBootRecord(const char* path, BootRecord& record) const;
  bool writeBootCount(uint8_t count);
  bool validBootRecord(const BootRecord& record) const;
  static uint32_t calculateCrc(const uint8_t* data, size_t length);
  static bool generationAfter(uint32_t left, uint32_t right);

  Diagnostics& diagnostics_;
  uint32_t generation_ = 0;
  int8_t activeSlot_ = -1;
  uint32_t bootGeneration_ = 0;
  int8_t bootActiveSlot_ = -1;
  uint8_t bootCount_ = 0;
  bool mounted_ = false;
};

}  // namespace svb
