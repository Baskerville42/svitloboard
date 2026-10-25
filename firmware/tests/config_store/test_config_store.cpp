#include <cassert>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <vector>

#include "AppConfig.h"
#include "ConfigStore.h"
#include "LittleFS.h"

HostLittleFS LittleFS;
void svb::Diagnostics::add(EventLevel, const String&) {}

namespace {
using namespace svb;

#pragma pack(push, 1)
// This is the released schema 1 layout, independent of the production reader.
struct ConfigV1 {
  uint8_t setupComplete;
  uint8_t pingPaused;
  uint8_t lastWifiProfile;
  char deviceName[33];
  char channelKey[65];
  char adminPassword[33];
  uint16_t pingIntervalSeconds;
  WifiProfile wifi[3];
  ExpertConfig expert;
};
#pragma pack(pop)

static_assert(sizeof(ConfigV1) == 440, "schema 1 wire layout changed");
static_assert(offsetof(AppConfig, pingSlotCount) == 698, "schema 2 prefix changed");
static_assert(sizeof(AppConfig) == 700, "schema 3 wire layout changed");

uint32_t crc32(const std::vector<uint8_t>& bytes, size_t size) {
  uint32_t crc = 0xffffffff;
  for (size_t index = 0; index < size; ++index) {
    crc ^= bytes[index];
    for (int bit = 0; bit < 8; ++bit) {
      crc = (crc & 1) ? ((crc >> 1) ^ 0xedb88320) : crc >> 1;
    }
  }
  return ~crc;
}

void write16(std::vector<uint8_t>& bytes, size_t at, uint16_t value) {
  bytes[at] = value & 0xff;
  bytes[at + 1] = value >> 8;
}
void write32(std::vector<uint8_t>& bytes, size_t at, uint32_t value) {
  for (int index = 0; index < 4; ++index) bytes[at + index] = value >> (8 * index);
}
void updateCrc(std::vector<uint8_t>& bytes) {
  write32(bytes, bytes.size() - 4, crc32(bytes, bytes.size() - 4));
}

AppConfig example(const char* name = "Saved-board") {
  AppConfig config;
  setConfigDefaults(config);
  config.setupComplete = 1;
  config.pingPaused = 1;
  config.lastWifiProfile = 2;
  strcpy(config.deviceName, name);
  strcpy(config.channelKey, "host-test-key");
  strcpy(config.adminPassword, "test-password");
  config.pingIntervalSeconds = 123;
  for (size_t index = 0; index < kWifiProfileCount; ++index) {
    config.wifi[index].enabled = 1;
    snprintf(config.wifi[index].ssid, sizeof(config.wifi[index].ssid), "network-%zu", index);
    strcpy(config.wifi[index].password, "wifi-password");
  }
  config.expert.enabled = 1;
  config.expert.offNotificationEnabled = 1;
  config.expert.mainsPin = 4;
  config.expert.mainsPresentLevel = 0;
  config.expert.useInternalPullup = 1;
  config.expert.debounceMs = 3456;
  assert(isConfigSane(config));
  return config;
}

std::vector<uint8_t> record(uint16_t schema, uint32_t generation, AppConfig config) {
  ConfigV1 legacy{};
  const void* payload = &config;
  size_t size = sizeof(config);
  if (schema == 1) {
    legacy.setupComplete = config.setupComplete;
    legacy.pingPaused = config.pingPaused;
    legacy.lastWifiProfile = config.lastWifiProfile;
    memcpy(legacy.deviceName, config.deviceName, sizeof(legacy.deviceName));
    memcpy(legacy.channelKey, config.channelKey, sizeof(legacy.channelKey));
    memcpy(legacy.adminPassword, config.adminPassword, sizeof(legacy.adminPassword));
    legacy.pingIntervalSeconds = config.pingIntervalSeconds;
    memcpy(legacy.wifi, config.wifi, sizeof(legacy.wifi));
    legacy.expert = config.expert;
    payload = &legacy;
    size = sizeof(legacy);
  } else if (schema == 2) {
    size = offsetof(AppConfig, pingSlotCount);
  }
  std::vector<uint8_t> bytes(12 + size + 4);
  write32(bytes, 0, 0x32564253);
  write16(bytes, 4, schema);
  write16(bytes, 6, bytes.size());
  write32(bytes, 8, generation);
  memcpy(bytes.data() + 12, payload, size);
  updateCrc(bytes);
  return bytes;
}

AppConfig loadExpected(const AppConfig& expected, uint32_t expectedGeneration) {
  Diagnostics diagnostics;
  ConfigStore store(diagnostics);
  assert(store.begin());
  AppConfig loaded;
  assert(store.load(loaded));
  assert(memcmp(&loaded, &expected, sizeof(loaded)) == 0);
  assert(store.generation() == expectedGeneration);
  return loaded;
}

void reset() { LittleFS = HostLittleFS{}; }

void testSchemaMigrationAndRoundTrip() {
  for (uint16_t schema = 1; schema <= 3; ++schema) {
    reset();
    AppConfig config = example();
    if (schema == 2) {
      config.reportingBackend = static_cast<uint8_t>(ReportingBackend::CustomUrl);
      strcpy(config.customUrl, "http://example.test/status?up=1");
    }
    if (schema == 3) {
      config.pingSlotCount = 4;
      config.pingSlotIndex = 3;
    }
    LittleFS.files["/cfg0.bin"] = record(schema, 17, config);
    loadExpected(config, schema == 3 ? 17 : 18);
    // Reboot reads the newly persisted record and does not migrate repeatedly.
    loadExpected(config, schema == 3 ? 17 : 18);
    if (schema < 3) {
      const auto& saved = LittleFS.files.at("/cfg1.bin");
      assert(saved[4] == 3);
      assert(saved.size() == 716);
    }
  }
}

void testNewestAcrossAllSchemas() {
  for (uint16_t older = 1; older <= 3; ++older) {
    for (uint16_t newer = 1; newer <= 3; ++newer) {
      for (int newerSlot = 0; newerSlot < 2; ++newerSlot) {
        reset();
        AppConfig oldConfig = example("Old-board");
        AppConfig newConfig = example("New-board");
        LittleFS.files[newerSlot ? "/cfg0.bin" : "/cfg1.bin"] = record(older, 8, oldConfig);
        LittleFS.files[newerSlot ? "/cfg1.bin" : "/cfg0.bin"] = record(newer, 9, newConfig);
        loadExpected(newConfig, newer == 3 ? 9 : 10);
      }
    }
  }
}

void testGenerationWrap() {
  for (uint16_t schema = 1; schema <= 3; ++schema) {
    reset();
    AppConfig previous = example("Before-wrap");
    AppConfig current = example("After-wrap");
    LittleFS.files["/cfg0.bin"] = record(3, 0xffffffff, previous);
    LittleFS.files["/cfg1.bin"] = record(schema, 0, current);
    loadExpected(current, schema == 3 ? 0 : 1);
  }
}

void testInvalidNewerFallback() {
  for (uint16_t schema = 1; schema <= 3; ++schema) {
    for (int failure = 0; failure < 7; ++failure) {
      reset();
      AppConfig expected = example("Valid-old");
      auto bytes = record(schema, 40, example("Invalid-new"));
      switch (failure) {
        case 0: bytes[30] ^= 1; break;  // CRC mismatch.
        case 1: bytes[0] ^= 1; updateCrc(bytes); break;
        case 2: bytes[4] = 99; updateCrc(bytes); break;
        case 3: bytes[6] ^= 1; updateCrc(bytes); break;
        case 4: bytes[12] = 2; updateCrc(bytes); break;  // Invalid config with correct CRC.
        case 5: bytes.pop_back(); break;
        case 6: bytes.push_back(0); break;
      }
      LittleFS.files["/cfg0.bin"] = record(3, 39, expected);
      LittleFS.files["/cfg1.bin"] = bytes;
      loadExpected(expected, 39);
    }
  }
}

void testMigrationFailureKeepsSource() {
  for (uint16_t schema = 1; schema < 3; ++schema) {
    for (bool renameFailure : {false, true}) {
      reset();
      AppConfig expected = example();
      const auto original = record(schema, 12, expected);
      LittleFS.files["/cfg0.bin"] = original;
      LittleFS.failWrites = !renameFailure;
      LittleFS.failRename = renameFailure;
      loadExpected(expected, 12);
      assert(LittleFS.files.at("/cfg0.bin") == original);
      LittleFS.failWrites = false;
      LittleFS.failRename = false;
      loadExpected(expected, 13);
    }
  }
}

void testSlotValidationAndDefaults() {
  AppConfig config;
  setConfigDefaults(config);
  assert(config.pingSlotCount == 0 && config.pingSlotIndex == 1 && isConfigSane(config));
  for (unsigned count = 0; count <= 5; ++count) {
    for (unsigned index = 0; index <= 5; ++index) {
      config.pingSlotCount = count;
      config.pingSlotIndex = index;
      const bool valid = count == 0 ? index == 1 : count >= 2 && count <= 4 && index >= 1 && index <= count;
      assert(isConfigSane(config) == valid);
    }
  }
  reset();
  Diagnostics diagnostics;
  ConfigStore store(diagnostics);
  assert(store.begin());
  assert(!store.load(config));
  assert(config.pingSlotCount == 0 && config.pingSlotIndex == 1 && isConfigSane(config));
  assert(store.save(config));
  assert(store.generation() == 1);
}
}  // namespace

int main() {
  testSchemaMigrationAndRoundTrip();
  testNewestAcrossAllSchemas();
  testGenerationWrap();
  testInvalidNewerFallback();
  testMigrationFailureKeepsSource();
  testSlotValidationAndDefaults();
  std::cout << "ConfigStore migration, CRC, generation and failure tests passed\n";
}
