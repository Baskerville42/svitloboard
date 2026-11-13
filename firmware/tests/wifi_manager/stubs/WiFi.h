#pragma once

#include <Arduino.h>
#include <vector>

enum wl_status_t {
  WL_IDLE_STATUS, WL_NO_SSID_AVAIL, WL_SCAN_COMPLETED, WL_CONNECTED,
  WL_CONNECT_FAILED, WL_CONNECTION_LOST, WL_DISCONNECTED,
};

enum wifi_mode_t { WIFI_OFF, WIFI_STA, WIFI_AP, WIFI_AP_STA };

struct HostAddress {
  explicit HostAddress(const char* value) : value(value) {}
  String toString() const { return value; }
  const char* value;
};

struct WifiAttempt {
  uint32_t startedAt;
  String ssid;
};

// Model Arduino's asynchronous status: begin/disconnect do not necessarily clear
// a failure from an earlier attempt before the next scan/association completes.
struct HostWiFi {
  void persistent(bool enabled) { persistentEnabled = enabled; }
  void setAutoReconnect(bool enabled) { autoReconnectEnabled = enabled; }
  void hostname(const char*) {}
  void setHostname(const char*) {}
  bool mode(wifi_mode_t next) { currentMode = next; return true; }
  wl_status_t begin(const char* ssid, const char*) {
    attempts.push_back({millis(), String(ssid)});
    associated = false;
    attempting = true;
    return currentStatus;
  }
  bool disconnect(bool) {
    ++disconnects;
    associated = false;
    attempting = false;
    if (currentStatus == WL_CONNECTED) currentStatus = WL_DISCONNECTED;
    return true;
  }
  wl_status_t status() {
    if (associated && !networkAvailable) {
      associated = false;
      currentStatus = WL_DISCONNECTED;
      attempting = false;
    }
    if (attempting) {
      const uint32_t elapsed = millis() - attempts.back().startedAt;
      if (!associated && elapsed >= associationDelayMs) {
        if (networkAvailable && attempts.back().ssid == availableSsid.c_str()) {
          associated = true;
          currentStatus = WL_IDLE_STATUS;  // ESP32 association precedes GOT_IP.
        } else {
          currentStatus = failureStatus;
          attempting = false;
        }
      }
      if (associated && dhcpAvailable && elapsed - associationDelayMs >= dhcpDelayMs) {
        currentStatus = WL_CONNECTED;
        attempting = false;
      }
    }
    return currentStatus;
  }
  uint8_t* BSSID() {
    status();
#if defined(ESP8266)
    // The ESP8266 accessor exposes configured BSSID data before association.
    return attempts.empty() ? nullptr : bssid;
#else
    // ESP32 queries esp_wifi_sta_get_ap_info(), which requires association.
    return associated ? bssid : nullptr;
#endif
  }
  bool softAP(const char*, const char*) { accessPoint = true; ++apStarts; return true; }
  bool softAPdisconnect(bool) { accessPoint = false; ++apStops; return true; }
  HostAddress localIP() const { return HostAddress("192.168.1.42"); }
  HostAddress softAPIP() const { return HostAddress("192.168.4.1"); }
  int32_t RSSI() const { return -52; }
  String SSID() const { return attempts.empty() ? String() : attempts.back().ssid; }

  bool persistentEnabled = true;
  bool autoReconnectEnabled = true;
  bool networkAvailable = false;
  bool associated = false;
  bool dhcpAvailable = true;
  bool attempting = false;
  bool accessPoint = false;
  unsigned apStarts = 0;
  unsigned apStops = 0;
  unsigned disconnects = 0;
  uint32_t associationDelayMs = 4000;
  uint32_t dhcpDelayMs = 0;
  uint8_t bssid[6] = {0x02, 0, 0, 0, 0, 1};
  String availableSsid = "home";
  wl_status_t currentStatus = WL_DISCONNECTED;
  wl_status_t failureStatus = WL_NO_SSID_AVAIL;
  wifi_mode_t currentMode = WIFI_OFF;
  std::vector<WifiAttempt> attempts;
};

extern HostWiFi WiFi;
