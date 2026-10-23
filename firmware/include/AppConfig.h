#pragma once

#include <Arduino.h>

#include "BuildConfig.h"

namespace svb {

constexpr size_t kWifiProfileCount = 3;
constexpr size_t kCustomUrlSize = 257;

enum class ReportingBackend : uint8_t {
  Svitlobot = 0,
  CustomUrl = 1,
};

#pragma pack(push, 1)
struct WifiProfile {
  uint8_t enabled;
  char ssid[33];
  char password[65];
};

struct ExpertConfig {
  uint8_t enabled;
  uint8_t offNotificationEnabled;
  int8_t mainsPin;
  uint8_t mainsPresentLevel;
  uint8_t useInternalPullup;
  uint16_t debounceMs;
};

struct AppConfig {
  uint8_t setupComplete;
  uint8_t pingPaused;
  uint8_t lastWifiProfile;
  uint8_t reportingBackend;
  char deviceName[33];
  char channelKey[65];
  char customUrl[kCustomUrlSize];
  char adminPassword[33];
  uint16_t pingIntervalSeconds;
  WifiProfile wifi[kWifiProfileCount];
  ExpertConfig expert;
};
#pragma pack(pop)

void setConfigDefaults(AppConfig& config);
bool isConfigSane(const AppConfig& config);
bool copyChecked(char* destination, size_t destinationSize, const String& value);
bool validDeviceName(const String& value);
bool validSsid(const String& value);
bool validWifiPassword(const String& value, bool allowEmpty = true);
bool validChannelKey(const String& value);
bool validCustomUrl(const String& value);
bool validAdminPassword(const String& value);
bool parseReportingBackend(const String& value, ReportingBackend& backend);
const char* reportingBackendName(const AppConfig& config);
bool reportingConfigurationValid(const AppConfig& config);
bool reportingUsesTls(const AppConfig& config);
String deviceId();
String accessPointName();

}  // namespace svb
