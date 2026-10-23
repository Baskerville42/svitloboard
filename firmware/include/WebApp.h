#pragma once

#include <Arduino.h>
#include <DNSServer.h>

#if defined(ESP8266)
#include <ESP8266WebServer.h>
using SvitloWebServer = ESP8266WebServer;
#else
#include <WebServer.h>
using SvitloWebServer = WebServer;
#endif

#include "AppConfig.h"
#include "ConfigStore.h"
#include "Diagnostics.h"
#include "PowerMonitor.h"
#include "SvitlobotClient.h"
#include "WifiManager.h"

namespace svb {

class WebApp {
 public:
  WebApp(AppConfig& config, ConfigStore& store, WifiManager& wifi,
         SvitlobotClient& api, PowerMonitor& power, Diagnostics& diagnostics)
      : config_(config), store_(store), wifi_(wifi), api_(api), power_(power),
        diagnostics_(diagnostics), server_(80) {}

  void begin();
  void loop();

 private:
  void registerRoutes();
  void handleStatus();
  void handleStatusFrame();
  void handleConfig();
  void handleSetup();
  void handleWifi();
#if !defined(SVB_PROFILE_MINIMAL)
  void handleSettings();
  void handleSecurity();
#endif
  void handleExpert();
  void handleScan();
  void handleAction();
  void handleOtaFinished();
  void handleOtaUpload();
  void handleNotFound();

  bool authorize(bool mutation);
  bool saveOrError(const AppConfig& candidate);
  void sendJson(int status, const String& body);
  void sendError(int status, const __FlashStringHelper* message);
  static String jsonEscape(const String& value);
  static bool parseInteger(const String& value, long minimum, long maximum, long& result);
  void inspectOtaChunk(const uint8_t* data, size_t length);
  void syncCaptiveDns();
  void scheduleWifiReconfigure();

  AppConfig& config_;
  ConfigStore& store_;
  WifiManager& wifi_;
  SvitlobotClient& api_;
  PowerMonitor& power_;
  Diagnostics& diagnostics_;
  SvitloWebServer server_;
  DNSServer dns_;
  bool dnsActive_ = false;
  bool restartPending_ = false;
  uint32_t restartAt_ = 0;
  bool eraseWifiBeforeRestart_ = false;
  bool wifiReconfigurePending_ = false;
  uint32_t wifiReconfigureAt_ = 0;
#if !defined(SVB_PROFILE_MINIMAL)
  bool provisioningHandoff_ = false;
  uint32_t provisioningHandoffUntil_ = 0;
#endif
  bool otaAuthorized_ = false;
  bool otaFailed_ = false;
  bool otaSucceeded_ = false;
  bool otaMarkerFound_ = false;
  size_t otaMarkerProgress_ = 0;
};

}  // namespace svb
