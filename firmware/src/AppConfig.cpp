#include "AppConfig.h"

#include <cstring>

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#include <esp_system.h>
#endif

namespace svb {

void setConfigDefaults(AppConfig& config) {
  memset(&config, 0, sizeof(config));
  copyChecked(config.deviceName, sizeof(config.deviceName), kDefaultDeviceName);
  config.pingIntervalSeconds = kDefaultPingIntervalSeconds;
  config.lastWifiProfile = 0;
  config.expert.mainsPin = -1;
  config.expert.mainsPresentLevel = HIGH;
  config.expert.debounceMs = 1500;
}

bool isConfigSane(const AppConfig& config) {
  if (config.setupComplete > 1 || config.pingPaused > 1 ||
      config.lastWifiProfile >= kWifiProfileCount) {
    return false;
  }
  if (config.pingIntervalSeconds < kMinimumPingIntervalSeconds ||
      config.pingIntervalSeconds > kMaximumPingIntervalSeconds) {
    return false;
  }
  if (config.deviceName[sizeof(config.deviceName) - 1] != '\0' ||
      config.channelKey[sizeof(config.channelKey) - 1] != '\0' ||
      config.adminPassword[sizeof(config.adminPassword) - 1] != '\0') {
    return false;
  }
  for (const auto& profile : config.wifi) {
    if (profile.enabled > 1 || profile.ssid[sizeof(profile.ssid) - 1] != '\0' ||
        profile.password[sizeof(profile.password) - 1] != '\0') {
      return false;
    }
    if (profile.enabled &&
        (!validSsid(String(profile.ssid)) || !validWifiPassword(String(profile.password)))) {
      return false;
    }
  }
  if (!validDeviceName(String(config.deviceName)) ||
      !validAdminPassword(String(config.adminPassword))) {
    return false;
  }
  if (config.setupComplete &&
      (!config.wifi[0].enabled || !validChannelKey(String(config.channelKey)))) {
    return false;
  }
  return config.expert.enabled <= 1 && config.expert.offNotificationEnabled <= 1 &&
         config.expert.mainsPin >= -1 && config.expert.mainsPin <= 48 &&
         config.expert.mainsPresentLevel <= 1 && config.expert.useInternalPullup <= 1 &&
         config.expert.debounceMs >= 100 && config.expert.debounceMs <= 30000;
}

bool copyChecked(char* destination, size_t destinationSize, const String& value) {
  if (destination == nullptr || destinationSize == 0 || value.length() >= destinationSize) {
    return false;
  }
  memcpy(destination, value.c_str(), value.length() + 1);
  return true;
}

bool validDeviceName(const String& value) {
  if (value.length() < 1 || value.length() > 32 || value[0] == '-' ||
      value[value.length() - 1] == '-') {
    return false;
  }
  for (size_t index = 0; index < value.length(); ++index) {
    const unsigned char c = static_cast<unsigned char>(value[index]);
    if (!(isalnum(c) || c == '-')) return false;
  }
  return true;
}

bool validSsid(const String& value) {
  return value.length() >= 1 && value.length() <= 32;
}

bool validWifiPassword(const String& value, bool allowEmpty) {
  if (value.length() == 0) return allowEmpty;
  if (value.length() >= 8 && value.length() <= 63) return true;
  if (value.length() != 64) return false;
  for (size_t index = 0; index < value.length(); ++index) {
    if (!isxdigit(static_cast<unsigned char>(value[index]))) return false;
  }
  return true;
}

bool validChannelKey(const String& value) {
  if (value.length() < 1 || value.length() > 64) return false;
  for (size_t i = 0; i < value.length(); ++i) {
    const unsigned char c = static_cast<unsigned char>(value[i]);
    if (c < 0x21 || c > 0x7E) return false;
  }
  return true;
}

bool validAdminPassword(const String& value) {
  if (value.isEmpty()) return true;
  if (value.length() < 8 || value.length() > 32) return false;
  for (size_t index = 0; index < value.length(); ++index) {
    const unsigned char c = static_cast<unsigned char>(value[index]);
    if (c < 0x20 || c > 0x7E) return false;
  }
  return true;
}

String deviceId() {
#if defined(ESP8266)
  return String(ESP.getChipId(), HEX);
#else
  const uint64_t mac = ESP.getEfuseMac();
  char buffer[13] = {};
  snprintf(buffer, sizeof(buffer), "%012llX", static_cast<unsigned long long>(mac & 0xFFFFFFFFFFFFULL));
  return String(buffer);
#endif
}

String accessPointName() {
  String id = deviceId();
  id.toUpperCase();
  if (id.length() > 6) id = id.substring(id.length() - 6);
  return String(kAccessPointPrefix) + id;
}

}  // namespace svb
