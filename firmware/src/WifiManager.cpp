#include "WifiManager.h"

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif

#include "BuildConfig.h"

namespace svb {

void WifiManager::begin() {
  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);

#if defined(ESP8266)
  WiFi.hostname(config_.deviceName);
#else
  WiFi.setHostname(config_.deviceName);
#endif

  if (!config_.setupComplete || configuredProfileCount() == 0) {
    WiFi.disconnect(false);
    startAccessPoint();
    state_ = State::NoCredentials;
    return;
  }
  reconnectNow();
}

void WifiManager::loop() {
  const uint32_t now = millis();

  if (WiFi.status() == WL_CONNECTED) {
    if (state_ != State::Connected) handleConnected(now);
#if !defined(SVB_PROFILE_MINIMAL)
    if (apActive_ && apStopAt_ && deadlineReached(now, apStopAt_)) stopAccessPoint();
#endif
    if (!stableProfileReported_ && activeProfile_ >= 0 &&
        deadlineReached(now, connectedAt_ + kWifiStableMs)) {
      stableProfileReported_ = true;
      stableProfilePending_ = config_.lastWifiProfile != activeProfile_;
      diagnostics_.add(EventLevel::Info,
                       String(F("Wi-Fi profile stable: ")) + (activeProfile_ + 1));
    }
    return;
  }

  if (state_ == State::Connected) {
    lastError_ = F("Wi-Fi link lost");
    diagnostics_.add(EventLevel::Warning, lastError_);
    activeProfile_ = -1;
    stableProfilePending_ = false;
    stableProfileReported_ = false;
    buildAttemptOrder();
    startNextAttempt(now);
    return;
  }

  const wl_status_t wifiStatus = WiFi.status();
  const bool terminalFailure = wifiStatus == WL_CONNECT_FAILED || wifiStatus == WL_NO_SSID_AVAIL;
  if (state_ == State::Connecting &&
      (deadlineReached(now, attemptDeadline_) ||
       (terminalFailure && deadlineReached(now, attemptStartedAt_ + kWifiTerminalStateGraceMs)))) {
    diagnostics_.add(EventLevel::Warning,
                     String(terminalFailure ? F("Wi-Fi rejected profile ")
                                            : F("Wi-Fi timeout for profile ")) +
                         (attemptedProfile_ + 1));
    WiFi.disconnect(false);
    if (!startNextAttempt(now)) scheduleBackoff(now);
    return;
  }

  if (state_ == State::Backoff && deadlineReached(now, backoffDeadline_)) {
    buildAttemptOrder();
    if (!startNextAttempt(now)) state_ = State::NoCredentials;
  }
}

void WifiManager::reconfigure() {
#if defined(ESP8266)
  WiFi.hostname(config_.deviceName);
#else
  WiFi.setHostname(config_.deviceName);
#endif
  if (!config_.setupComplete || configuredProfileCount() == 0) {
    WiFi.disconnect(false);
    startAccessPoint();
    state_ = State::NoCredentials;
    activeProfile_ = -1;
    return;
  }
  reconnectNow();
}

void WifiManager::reconnectNow() {
  WiFi.disconnect(false);
  activeProfile_ = -1;
  stableProfilePending_ = false;
  stableProfileReported_ = false;
  backoffMs_ = kWifiInitialBackoffMs;
  buildAttemptOrder();
  if (!startNextAttempt(millis())) {
    state_ = State::NoCredentials;
    startAccessPoint();
  }
}

bool WifiManager::connected() const {
  return WiFi.status() == WL_CONNECTED;
}

const char* WifiManager::stateName() const {
  switch (state_) {
    case State::NoCredentials: return "setup";
    case State::Connecting: return "connecting";
    case State::Connected: return "connected";
    case State::Backoff: return "recovery";
  }
  return "unknown";
}

int32_t WifiManager::rssi() const {
  return connected() ? WiFi.RSSI() : 0;
}

String WifiManager::stationIp() const {
  return connected() ? WiFi.localIP().toString() : String();
}

String WifiManager::accessPointIp() const {
  return apActive_ ? WiFi.softAPIP().toString() : String();
}

String WifiManager::connectedSsid() const {
  return connected() ? WiFi.SSID() : String();
}

bool WifiManager::consumeNewStableProfile(uint8_t& profile) {
  if (!stableProfilePending_ || activeProfile_ < 0) return false;
  stableProfilePending_ = false;
  profile = static_cast<uint8_t>(activeProfile_);
  return true;
}

void WifiManager::buildAttemptOrder() {
  attemptCount_ = 0;
  attemptCursor_ = 0;
  const uint8_t preferred = config_.lastWifiProfile < kWifiProfileCount
                                ? config_.lastWifiProfile
                                : 0;
  if (config_.wifi[preferred].enabled && config_.wifi[preferred].ssid[0] != '\0') {
    attemptOrder_[attemptCount_++] = preferred;
  }
  for (uint8_t index = 0; index < kWifiProfileCount; ++index) {
    if (index != preferred && config_.wifi[index].enabled &&
        config_.wifi[index].ssid[0] != '\0') {
      attemptOrder_[attemptCount_++] = index;
    }
  }
}

bool WifiManager::startNextAttempt(uint32_t now) {
  if (attemptCursor_ >= attemptCount_) return false;
  beginAttempt(attemptOrder_[attemptCursor_++], now);
  return true;
}

void WifiManager::beginAttempt(uint8_t profile, uint32_t now) {
  attemptedProfile_ = static_cast<int8_t>(profile);
  state_ = State::Connecting;
  attemptStartedAt_ = now;
  attemptDeadline_ = now + kWifiAttemptMs;
  lastError_ = String();
  WiFi.mode(apActive_ ? WIFI_AP_STA : WIFI_STA);
  const WifiProfile& candidate = config_.wifi[profile];
  diagnostics_.add(EventLevel::Info,
                   String(F("Connecting Wi-Fi profile ")) + (profile + 1) + F(": ") +
                       candidate.ssid);
  WiFi.begin(candidate.ssid, candidate.password);
}

void WifiManager::handleConnected(uint32_t now) {
  state_ = State::Connected;
  activeProfile_ = attemptedProfile_;
  if (activeProfile_ < 0) {
    const String ssid = WiFi.SSID();
    for (uint8_t index = 0; index < kWifiProfileCount; ++index) {
      if (config_.wifi[index].enabled && ssid == config_.wifi[index].ssid) {
        activeProfile_ = static_cast<int8_t>(index);
        break;
      }
    }
  }
  connectedAt_ = now;
  stableProfilePending_ = false;
  stableProfileReported_ = activeProfile_ >= 0 && config_.lastWifiProfile == activeProfile_;
  backoffMs_ = kWifiInitialBackoffMs;
  lastError_ = String();
  diagnostics_.add(EventLevel::Info,
                   String(F("Wi-Fi connected: ")) + WiFi.SSID() + F(" at ") +
                       WiFi.localIP().toString());
#if defined(SVB_PROFILE_MINIMAL)
  if (config_.setupComplete) stopAccessPoint();
#else
  if (config_.setupComplete && apActive_) {
    apStopAt_ = now + kProvisioningApGraceMs;
    diagnostics_.add(EventLevel::Info, F("Provisioning AP will stop after handoff"));
  }
#endif
}

void WifiManager::scheduleBackoff(uint32_t now) {
  state_ = State::Backoff;
  attemptedProfile_ = -1;
  lastError_ = F("All configured Wi-Fi networks are unavailable");
  diagnostics_.add(EventLevel::Warning, lastError_);
  startAccessPoint();
  backoffDeadline_ = now + backoffMs_;
  backoffMs_ = min<uint32_t>(backoffMs_ * 2U, kWifiMaximumBackoffMs);
}

void WifiManager::startAccessPoint() {
  if (apActive_) return;
#if !defined(SVB_PROFILE_MINIMAL)
  apStopAt_ = 0;
#endif
  WiFi.mode(WIFI_AP_STA);
  const String name = accessPointName();
  if (WiFi.softAP(name.c_str(), kAccessPointPassword)) {
    apActive_ = true;
    diagnostics_.add(EventLevel::Info,
                     String(F("Recovery AP started: ")) + name + F(" / ") +
                         kAccessPointPassword);
  } else {
    diagnostics_.add(EventLevel::Error, F("Cannot start recovery AP"));
  }
}

void WifiManager::stopAccessPoint() {
  if (!apActive_) return;
  WiFi.softAPdisconnect(false);
  apActive_ = false;
#if !defined(SVB_PROFILE_MINIMAL)
  apStopAt_ = 0;
#endif
  WiFi.mode(WIFI_STA);
  diagnostics_.add(EventLevel::Info, F("Recovery AP stopped"));
}

uint8_t WifiManager::configuredProfileCount() const {
  uint8_t result = 0;
  for (const auto& profile : config_.wifi) {
    if (profile.enabled && profile.ssid[0] != '\0') ++result;
  }
  return result;
}

}  // namespace svb
