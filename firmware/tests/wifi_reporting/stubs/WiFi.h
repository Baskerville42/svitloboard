#pragma once

#include <Arduino.h>
#include <vector>

enum wl_status_t {
  WL_IDLE_STATUS, WL_NO_SSID_AVAIL, WL_SCAN_COMPLETED, WL_CONNECTED,
  WL_CONNECT_FAILED, WL_CONNECTION_LOST, WL_DISCONNECTED,
};
enum wifi_mode_t { WIFI_OFF, WIFI_STA, WIFI_AP, WIFI_AP_STA };

struct HostAddress {
  explicit HostAddress(const char* text) : text(text) {}
  String toString() const { return text; }
  const char* text;
};

struct WifiAttempt {
  uint32_t startedAt;
  String ssid;
  wifi_mode_t mode;
};

// Only the radio boundary is simulated. In particular, begin() leaves the old
// status untouched, as Arduino ESP32 does while its scan runs asynchronously.
struct HostWiFi {
  void persistent(bool) {}
  void setAutoReconnect(bool) {}
  void setHostname(const char*) {}
  bool mode(wifi_mode_t next) { currentMode = next; return true; }
  wl_status_t begin(const char* ssid, const char*) {
    attempts.push_back({millis(), String(ssid), currentMode});
    attempting = true;
    associated = false;
    return currentStatus;
  }
  bool disconnect(bool) {
    attempting = associated = false;
    if (currentStatus == WL_CONNECTED) currentStatus = WL_DISCONNECTED;
    return true;
  }
  wl_status_t status() {
    if (currentStatus == WL_CONNECTED && !routerAvailable) {
      currentStatus = WL_DISCONNECTED;
      associated = attempting = false;
    }
    if (attempting) {
      const uint32_t age = millis() - attempts.back().startedAt;
      if (!associated && age >= associationMs) {
        if (!routerAvailable || !(attempts.back().ssid == "home")) {
          currentStatus = WL_NO_SSID_AVAIL;
          attempting = false;
        } else {
          associated = true;
          currentStatus = WL_IDLE_STATUS;
          ++associations;
        }
      }
      if (attempting && associated && age >= associationMs + dhcpMs) {
        currentStatus = WL_CONNECTED;
        attempting = false;
        ++leases;
      }
    }
    return currentStatus;
  }
  uint8_t* BSSID() {
    status();
    static uint8_t bssid[6] = {2, 3, 4, 5, 6, 7};
    return associated ? bssid : nullptr;
  }
  bool softAP(const char*, const char*) { ap = true; ++apStarts; return true; }
  bool softAPdisconnect(bool) { ap = false; ++apStops; return true; }
  HostAddress localIP() const { return HostAddress("192.168.1.42"); }
  HostAddress softAPIP() const { return HostAddress("192.168.4.1"); }
  int32_t RSSI() const { return -52; }
  String SSID() const { return attempts.empty() ? String() : attempts.back().ssid; }

  bool routerAvailable = false;
  bool attempting = false;
  bool associated = false;
  bool ap = false;
  unsigned associations = 0, leases = 0, apStarts = 0, apStops = 0;
  uint32_t associationMs = 4500, dhcpMs = 2500;
  wl_status_t currentStatus = WL_DISCONNECTED;
  wifi_mode_t currentMode = WIFI_OFF;
  std::vector<WifiAttempt> attempts;
};
extern HostWiFi WiFi;

class WiFiClient {
 public:
  virtual ~WiFiClient() = default;
  bool secure = false;
  bool trusted = false;
  bool timeoutSet = false;
};
