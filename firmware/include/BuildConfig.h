#pragma once

#include <Arduino.h>

#ifndef SVB_FIRMWARE_VERSION
#define SVB_FIRMWARE_VERSION "2.2.0-dev"
#endif

#ifndef SVB_BOARD_NAME
#define SVB_BOARD_NAME "unknown"
#endif

#ifndef SVB_GIT_COMMIT
#define SVB_GIT_COMMIT "unknown"
#endif

#ifndef SVB_ENABLE_TLS
#define SVB_ENABLE_TLS 1
#endif

#ifndef SVB_ENABLE_OTA
#define SVB_ENABLE_OTA 0
#endif

#ifndef SVB_ENABLE_EXPERT
#define SVB_ENABLE_EXPERT 0
#endif

namespace svb {

constexpr char kFirmwareVersion[] = SVB_FIRMWARE_VERSION;
constexpr char kBoardName[] = SVB_BOARD_NAME;
constexpr char kGitCommit[] = SVB_GIT_COMMIT;
constexpr char kOtaCompatibilityMarker[] = "SVB2-OTA:" SVB_BOARD_NAME;
constexpr char kDefaultDeviceName[] = "SvitloBoard";
constexpr char kAccessPointPrefix[] = "SvitloBoard-";
constexpr char kAccessPointPassword[] = "svitlobot";
constexpr char kApiHost[] = "api.svitlobot.in.ua";
constexpr uint16_t kApiPort = 443;
constexpr uint16_t kDefaultPingIntervalSeconds = 60;
constexpr uint16_t kMinimumPingIntervalSeconds = 30;
constexpr uint16_t kMaximumPingIntervalSeconds = 3600;
constexpr uint32_t kWifiAttemptMs = 12000;
constexpr uint32_t kWifiTerminalStateGraceMs = 1500;
constexpr uint32_t kWifiStableMs = 30000;
constexpr uint32_t kWifiInitialBackoffMs = 15000;
constexpr uint32_t kWifiMaximumBackoffMs = 300000;
constexpr uint32_t kWifiReconfigureDelayMs = 2000;
constexpr uint32_t kProvisioningApGraceMs = 60000;
constexpr uint32_t kProvisioningStatusMs = 60000;
constexpr uint32_t kHttpTimeoutMs = 6500;
constexpr uint32_t kPostSavePingDelayMs = 2500;
constexpr uint32_t kValidUnixTime = 1700000000UL;
constexpr uint8_t kFastBootResetThreshold = 4;
constexpr uint32_t kFastBootWindowMs = 8000;

inline bool deadlineReached(uint32_t now, uint32_t deadline) {
  return static_cast<int32_t>(now - deadline) >= 0;
}

}  // namespace svb
