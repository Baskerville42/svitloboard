#pragma once

#include <Arduino.h>

#include "AppConfig.h"
#include "Diagnostics.h"

namespace svb {

class WifiManager {
 public:
  enum class State : uint8_t { NoCredentials, Connecting, Connected, Backoff };

  WifiManager(AppConfig& config, Diagnostics& diagnostics)
      : config_(config), diagnostics_(diagnostics) {}

  void begin();
  void loop();
  void reconfigure();
  void reconnectNow();

  bool connected() const;
  bool apActive() const { return apActive_; }
  State state() const { return state_; }
  const char* stateName() const;
  int8_t activeProfile() const { return activeProfile_; }
  int8_t attemptedProfile() const { return attemptedProfile_; }
  int32_t rssi() const;
  String stationIp() const;
  String accessPointIp() const;
  String connectedSsid() const;
  String lastError() const { return lastError_; }
  bool consumeNewStableProfile(uint8_t& profile);

 private:
  void buildAttemptOrder();
  bool startNextAttempt(uint32_t now);
  void beginAttempt(uint8_t profile, uint32_t now);
  void handleConnected(uint32_t now);
  void scheduleBackoff(uint32_t now);
  void startAccessPoint();
  void stopAccessPoint();
  uint8_t configuredProfileCount() const;

  AppConfig& config_;
  Diagnostics& diagnostics_;
  State state_ = State::NoCredentials;
  uint8_t attemptOrder_[kWifiProfileCount]{};
  uint8_t attemptCount_ = 0;
  uint8_t attemptCursor_ = 0;
  int8_t attemptedProfile_ = -1;
  int8_t activeProfile_ = -1;
  uint32_t attemptDeadline_ = 0;
  uint32_t attemptStartedAt_ = 0;
  uint32_t connectedAt_ = 0;
  uint32_t backoffDeadline_ = 0;
  uint32_t backoffMs_ = kWifiInitialBackoffMs;
  bool apActive_ = false;
  bool stableProfilePending_ = false;
  bool stableProfileReported_ = false;
  String lastError_;
};

}  // namespace svb
