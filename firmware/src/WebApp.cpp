#include "WebApp.h"

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <Updater.h>
#else
#include <Update.h>
#include <WiFi.h>
#endif

#include "BuildConfig.h"
#include "WebUiGzip.h"

namespace svb {

void WebApp::begin() {
#if defined(ESP8266)
  server_.collectHeaders("X-Svitloboard-Action");
#else
  const char* collectedHeaders[] = {"X-Svitloboard-Action"};
  server_.collectHeaders(collectedHeaders, 1);
#endif
  registerRoutes();
  server_.begin();
  syncCaptiveDns();
  diagnostics_.add(EventLevel::Info, F("HTTP server started"));
}

void WebApp::loop() {
  syncCaptiveDns();
  if (dnsActive_) dns_.processNextRequest();
  server_.handleClient();
  const uint32_t now = millis();
  if (wifiReconfigurePending_ && deadlineReached(now, wifiReconfigureAt_)) {
    wifiReconfigurePending_ = false;
    wifi_.reconfigure();
  }
  if (restartPending_ && deadlineReached(now, restartAt_)) {
    if (eraseWifiBeforeRestart_) WiFi.disconnect(true, true);
    ESP.restart();
  }
}

void WebApp::registerRoutes() {
  server_.on("/", HTTP_GET, [this]() {
    if (!authorize(false)) return;
    server_.sendHeader(F("Cache-Control"), F("no-store"));
    server_.sendHeader(F("Content-Encoding"), F("gzip"));
    server_.send_P(200, "text/html; charset=utf-8",
                   reinterpret_cast<PGM_P>(kWebUiGzip), kWebUiGzipSize);
  });
  server_.on("/api/status", HTTP_GET, [this]() { handleStatus(); });
  server_.on("/status-frame", HTTP_GET, [this]() { handleStatusFrame(); });
  server_.on("/api/config", HTTP_GET, [this]() { handleConfig(); });
  server_.on("/api/setup", HTTP_POST, [this]() { handleSetup(); });
  server_.on("/api/wifi", HTTP_POST, [this]() { handleWifi(); });
#if defined(SVB_PROFILE_MINIMAL)
  server_.on("/api/reporting", HTTP_POST, [this]() { handleReporting(); });
#endif
#if !defined(SVB_PROFILE_MINIMAL)
  server_.on("/api/settings", HTTP_POST, [this]() { handleSettings(); });
#endif
  server_.on("/api/security", HTTP_POST, [this]() { handleSecurity(); });
  server_.on("/api/expert", HTTP_POST, [this]() { handleExpert(); });
  server_.on("/api/wifi/scan", HTTP_POST, [this]() { handleScan(); });
  server_.on("/api/action", HTTP_POST, [this]() { handleAction(); });
#if SVB_ENABLE_OTA
  server_.on("/api/ota", HTTP_POST, [this]() { handleOtaFinished(); },
             [this]() { handleOtaUpload(); });
#endif
  server_.on("/generate_204", HTTP_GET, [this]() { handleNotFound(); });
  server_.on("/hotspot-detect.html", HTTP_GET, [this]() { handleNotFound(); });
  server_.onNotFound([this]() { handleNotFound(); });
}

void WebApp::handleStatusFrame() {
  if (!authorize(false)) return;
  if (!config_.setupComplete) {
    sendError(409, F("Setup is incomplete"));
    return;
  }
  if (config_.reportingBackend == static_cast<uint8_t>(ReportingBackend::CustomUrl)) {
    String html;
    html.reserve(520);
    html += F("<!doctype html><html lang=uk><meta charset=utf-8>"
              "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
              "<style>body{font:16px system-ui;color:#625d72;padding:22px;margin:0}"
              "b{color:#252236}</style><body><b>Custom URL</b><p>Стан запитів: ");
    html += api_.resultName();
    html += F("</p><p>HTTP-код: ");
    html += api_.httpCode();
    html += F("</p><p>Custom URL викликається лише за розкладом або вручну кнопкою ping. "
              "Ця перевірка не надсилає додатковий запит.</p></body></html>");
    server_.sendHeader(F("Cache-Control"), F("no-store"));
    server_.send(200, "text/html; charset=utf-8", html);
    return;
  }
  if (config_.channelKey[0] == '\0') {
    server_.sendHeader(F("Cache-Control"), F("no-store"));
    server_.send(200, "text/html; charset=utf-8",
                 F("<!doctype html><html lang=uk><meta charset=utf-8>"
                   "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
                   "<style>body{font:16px system-ui;color:#625d72;padding:22px;margin:0}"
                   "b{color:#252236}</style><body><b>Ключ каналу не налаштовано</b>"
                   "<p>Додайте його в секції «СвітлоБот».</p></body></html>"));
    return;
  }
  server_.sendHeader(F("Cache-Control"), F("no-store"));
  server_.sendHeader(F("Location"), api_.statusPageUrl(), true);
  server_.send(302, "text/plain", "");
}

void WebApp::handleStatus() {
#if defined(SVB_PROFILE_MINIMAL)
  const bool handoffVisible = provisioningHandoff_ && wifi_.apActive() &&
                              !deadlineReached(millis(), provisioningHandoffUntil_);
  if (!handoffVisible && !authorize(false)) return;
  String json;
  json.reserve(620);
  json += F("{\"version\":\"");
  json += kFirmwareVersion;
  json += F("\",\"board\":\"");
  json += kBoardName;
  json += F("\",\"commit\":\"");
  json += kGitCommit;
  json += F("\",\"configGeneration\":");
  json += store_.generation();
  json += F(",\"features\":{\"ota\":false,\"expert\":false,\"settings\":false,"
            "\"security\":true,\"wifiProfiles\":3,\"logs\":true},"
            "\"wifi\":{\"state\":\"");
  json += wifi_.stateName();
  json += F("\",\"connected\":");
  json += wifi_.connected() ? F("true") : F("false");
  json += F(",\"ssid\":\"");
  json += jsonEscape(wifi_.connectedSsid());
  json += F("\",\"ip\":\"");
  json += wifi_.stationIp();
  json += F("\",\"apActive\":");
  json += wifi_.apActive() ? F("true") : F("false");
  json += F(",\"apIp\":\"");
  json += wifi_.accessPointIp();
  json += F("\"},\"api\":{\"backend\":\"");
  json += reportingBackendName(config_);
  json += F("\",\"result\":\"");
  json += api_.resultName();
  json += F("\",\"httpCode\":");
  json += api_.httpCode();
  json += F("},\"events\":[");
  for (uint8_t index = 0; index < diagnostics_.count(); ++index) {
    if (index) json += ',';
    const DiagnosticEvent& event = diagnostics_.newest(index);
    json += F("{\"ms\":");
    json += event.uptimeMs;
    json += F(",\"level\":\"");
    json += eventLevelName(event.level);
    json += F("\",\"message\":\"");
    json += jsonEscape(event.message);
    json += F("\"}");
  }
  json += F("]}");
  sendJson(200, json);
#else
  const bool handoffVisible = provisioningHandoff_ && wifi_.apActive() &&
                              !deadlineReached(millis(), provisioningHandoffUntil_);
  if (!handoffVisible && !authorize(false)) return;
  String json;
  json.reserve(2740);
  json += F("{\"version\":\"");
  json += kFirmwareVersion;
  json += F("\",\"board\":\"");
  json += kBoardName;
  json += F("\",\"commit\":\"");
  json += kGitCommit;
  json += F("\",\"uptimeMs\":");
  json += millis();
  json += F(",\"freeHeap\":");
  json += ESP.getFreeHeap();
  json += F(",\"configGeneration\":");
  json += store_.generation();
  json += F(",\"features\":{\"ota\":");
  json += SVB_ENABLE_OTA ? F("true") : F("false");
  json += F(",\"expert\":");
  json += SVB_ENABLE_EXPERT ? F("true") : F("false");
  json += F(",\"settings\":true,\"security\":true,\"wifiProfiles\":3,\"logs\":true},"
            "\"wifi\":{\"state\":\"");
  json += wifi_.stateName();
  json += F("\",\"connected\":");
  json += wifi_.connected() ? F("true") : F("false");
  json += F(",\"ssid\":\"");
  json += jsonEscape(wifi_.connectedSsid());
  json += F("\",\"ip\":\"");
  json += wifi_.stationIp();
  json += F("\",\"rssi\":");
  json += wifi_.rssi();
  json += F(",\"profile\":");
  json += wifi_.activeProfile();
  json += F(",\"apActive\":");
  json += wifi_.apActive() ? F("true") : F("false");
  json += F(",\"apIp\":\"");
  json += wifi_.accessPointIp();
  json += F("\",\"error\":\"");
  json += jsonEscape(wifi_.lastError());
  json += F("\"},\"api\":{\"backend\":\"");
  json += reportingBackendName(config_);
  json += F("\",\"result\":\"");
  json += api_.resultName();
  json += F("\",\"httpCode\":");
  json += api_.httpCode();
  json += F(",\"lastAttemptMs\":");
  json += api_.lastAttemptMs();
  json += F(",\"lastSuccessMs\":");
  json += api_.lastSuccessMs();
  json += F(",\"clockReady\":");
  json += api_.clockReady() ? F("true") : F("false");
  json += F(",\"usesTls\":");
  json += reportingUsesTls(config_) ? F("true") : F("false");
  json += F(",\"offSupported\":");
  json += config_.reportingBackend == static_cast<uint8_t>(ReportingBackend::Svitlobot)
              ? F("true")
              : F("false");
  json += F(",\"offQueued\":");
  json += api_.offQueued() ? F("true") : F("false");
  json += F("},\"power\":{\"available\":");
  json += power_.available() ? F("true") : F("false");
  json += F(",\"mainsPresent\":");
  json += power_.mainsPresent() ? F("true") : F("false");
  json += F(",\"error\":\"");
  json += jsonEscape(power_.validationError());
  json += F("\"},\"events\":[");
  for (uint8_t index = 0; index < diagnostics_.count(); ++index) {
    if (index) json += ',';
    const DiagnosticEvent& event = diagnostics_.newest(index);
    json += F("{\"ms\":");
    json += event.uptimeMs;
    json += F(",\"level\":\"");
    json += eventLevelName(event.level);
    json += F("\",\"message\":\"");
    json += jsonEscape(event.message);
    json += F("\"}");
  }
  json += F("]}");
  sendJson(200, json);
#endif
}

void WebApp::handleConfig() {
  if (!authorize(false)) return;
#if defined(SVB_PROFILE_MINIMAL)
  String json;
  json.reserve(720);
  json += F("{\"setupComplete\":");
  json += config_.setupComplete ? F("true") : F("false");
  json += F(",\"deviceName\":\"");
  json += jsonEscape(config_.deviceName);
  json += F("\",\"adminPasswordSet\":");
  json += config_.adminPassword[0] ? F("true") : F("false");
  json += F(",\"reportingBackend\":\"");
  json += reportingBackendName(config_);
  json += F("\",\"channelKey\":\"");
  json += jsonEscape(config_.channelKey);
  json += F("\",\"customUrl\":\"");
  json += jsonEscape(config_.customUrl);
  json += F("\",\"pingPaused\":");
  json += config_.pingPaused ? F("true") : F("false");
  json += F(",\"pingInterval\":");
  json += config_.pingIntervalSeconds;
  json += F(",\"wifi\":[");
  for (uint8_t index = 0; index < kWifiProfileCount; ++index) {
    if (index) json += ',';
    json += F("{\"enabled\":");
    json += config_.wifi[index].enabled ? F("true") : F("false");
    json += F(",\"ssid\":\"");
    json += jsonEscape(config_.wifi[index].ssid);
    json += F("\",\"hasPassword\":");
    json += config_.wifi[index].password[0] ? F("true") : F("false");
    json += '}';
  }
  json += F("]}");
  sendJson(200, json);
#else
  String json;
  json.reserve(1350);
  json += F("{\"setupComplete\":");
  json += config_.setupComplete ? F("true") : F("false");
  json += F(",\"deviceName\":\"");
  json += jsonEscape(config_.deviceName);
  json += F("\",\"hasChannelKey\":");
  json += config_.channelKey[0] ? F("true") : F("false");
  json += F(",\"channelKey\":\"");
  json += jsonEscape(config_.channelKey);
  json += '"';
  json += F(",\"reportingBackend\":\"");
  json += reportingBackendName(config_);
  json += F("\",\"customUrl\":\"");
  json += jsonEscape(config_.customUrl);
  json += '"';
  json += F(",\"adminPasswordSet\":");
  json += config_.adminPassword[0] ? F("true") : F("false");
  json += F(",\"pingPaused\":");
  json += config_.pingPaused ? F("true") : F("false");
  json += F(",\"pingInterval\":");
  json += config_.pingIntervalSeconds;
  json += F(",\"wifi\":[");
  for (uint8_t index = 0; index < kWifiProfileCount; ++index) {
    if (index) json += ',';
    json += F("{\"enabled\":");
    json += config_.wifi[index].enabled ? F("true") : F("false");
    json += F(",\"ssid\":\"");
    json += jsonEscape(config_.wifi[index].ssid);
    json += F("\",\"hasPassword\":");
    json += config_.wifi[index].password[0] ? F("true") : F("false");
    json += '}';
  }
  json += F("],\"expert\":{\"enabled\":");
  json += config_.expert.enabled ? F("true") : F("false");
  json += F(",\"offEnabled\":");
  json += config_.expert.offNotificationEnabled ? F("true") : F("false");
  json += F(",\"pin\":");
  json += config_.expert.mainsPin;
  json += F(",\"presentLevel\":");
  json += config_.expert.mainsPresentLevel;
  json += F(",\"pullup\":");
  json += config_.expert.useInternalPullup ? F("true") : F("false");
  json += F(",\"debounce\":");
  json += config_.expert.debounceMs;
  json += F("}}");
  sendJson(200, json);
#endif
}

void WebApp::handleSetup() {
  if (!authorize(true)) return;
  const String ssid = server_.arg("ssid");
  const String password = server_.arg("password");
  const String channelKey = server_.arg("channelKey");
  const String customUrl = server_.arg("customUrl");
  const String adminPassword = server_.arg("adminPassword");
  String backendValue = server_.arg("reportingBackend");
  if (backendValue.isEmpty()) backendValue = F("svitlobot");
  ReportingBackend backend = ReportingBackend::Svitlobot;
  if (!validSsid(ssid) || !validWifiPassword(password) ||
      !parseReportingBackend(backendValue, backend) ||
      (!channelKey.isEmpty() && !validChannelKey(channelKey)) ||
      ((backend == ReportingBackend::CustomUrl || !customUrl.isEmpty()) &&
       !validCustomUrl(customUrl)) ||
      !validAdminPassword(adminPassword)) {
    sendError(400, F("Invalid Wi-Fi, reporting destination or administrator password"));
    return;
  }

  AppConfig candidate = config_;
  copyChecked(candidate.wifi[0].ssid, sizeof(candidate.wifi[0].ssid), ssid);
  copyChecked(candidate.wifi[0].password, sizeof(candidate.wifi[0].password), password);
  candidate.reportingBackend = static_cast<uint8_t>(backend);
  copyChecked(candidate.channelKey, sizeof(candidate.channelKey), channelKey);
  copyChecked(candidate.customUrl, sizeof(candidate.customUrl), customUrl);
  copyChecked(candidate.adminPassword, sizeof(candidate.adminPassword), adminPassword);
  candidate.wifi[0].enabled = 1;
  candidate.setupComplete = 1;
  if (!saveOrError(candidate)) return;
  provisioningHandoff_ = true;
  provisioningHandoffUntil_ = millis() + kProvisioningStatusMs;
  sendJson(200, F("{\"ok\":true}"));
  api_.requestPingAfter(kPostSavePingDelayMs);
  scheduleWifiReconfigure();
}

#if defined(SVB_PROFILE_MINIMAL)
void WebApp::handleReporting() {
  if (!authorize(true)) return;
  const String channelKey = server_.arg("channelKey");
  const String customUrl = server_.arg("customUrl");
  ReportingBackend backend = ReportingBackend::Svitlobot;
  long interval = 0;
  if (!parseReportingBackend(server_.arg("reportingBackend"), backend) ||
      (!channelKey.isEmpty() && !validChannelKey(channelKey)) ||
      ((backend == ReportingBackend::CustomUrl || !customUrl.isEmpty()) &&
       !validCustomUrl(customUrl)) ||
      !parseInteger(server_.arg("pingInterval"), kMinimumPingIntervalSeconds,
                    kMaximumPingIntervalSeconds, interval)) {
    sendError(400, F("Invalid reporting settings"));
    return;
  }

  AppConfig candidate = config_;
  candidate.reportingBackend = static_cast<uint8_t>(backend);
  copyChecked(candidate.channelKey, sizeof(candidate.channelKey), channelKey);
  copyChecked(candidate.customUrl, sizeof(candidate.customUrl), customUrl);
  candidate.pingPaused = server_.hasArg("pingPaused");
  candidate.pingIntervalSeconds = static_cast<uint16_t>(interval);
  if (!saveOrError(candidate)) return;
  sendJson(200, F("{\"ok\":true}"));
  api_.requestPingAfter(kPostSavePingDelayMs);
}
#endif

void WebApp::handleWifi() {
  if (!authorize(true)) return;
  long indexLong = -1;
  if (!parseInteger(server_.arg("index"), 0, kWifiProfileCount - 1, indexLong)) {
    sendError(400, F("Invalid Wi-Fi profile index"));
    return;
  }
  const uint8_t index = static_cast<uint8_t>(indexLong);
  const String ssid = server_.arg("ssid");
  const String password = server_.arg("password");
  const bool clearPassword = server_.hasArg("clearPassword");
  const bool enabled = index == 0 || server_.hasArg("enabled");
  if ((enabled && !validSsid(ssid)) || (!password.isEmpty() && !validWifiPassword(password)) ||
      (clearPassword && !password.isEmpty())) {
    sendError(400, F("Invalid SSID or Wi-Fi password"));
    return;
  }
  if (enabled) {
    for (uint8_t other = 0; other < kWifiProfileCount; ++other) {
      if (other != index && config_.wifi[other].ssid[0] != '\0' &&
          ssid == config_.wifi[other].ssid) {
        sendError(409, F("This Wi-Fi network is already assigned to another profile"));
        return;
      }
    }
  }

  AppConfig candidate = config_;
  candidate.wifi[index].enabled = enabled;
  if (!enabled && index > 0) {
    memset(&candidate.wifi[index], 0, sizeof(candidate.wifi[index]));
  } else {
    const bool ssidChanged = ssid != candidate.wifi[index].ssid;
    if (ssidChanged && password.isEmpty() && candidate.wifi[index].password[0] != '\0' &&
        !clearPassword) {
      sendError(400, F("Enter the password for the new SSID or mark it as an open network"));
      return;
    }
    copyChecked(candidate.wifi[index].ssid, sizeof(candidate.wifi[index].ssid), ssid);
    if (clearPassword) {
      memset(candidate.wifi[index].password, 0, sizeof(candidate.wifi[index].password));
    } else if (!password.isEmpty()) {
      copyChecked(candidate.wifi[index].password, sizeof(candidate.wifi[index].password), password);
    }
  }
  if (!saveOrError(candidate)) return;
  sendJson(200, F("{\"ok\":true}"));
  scheduleWifiReconfigure();
}

#if !defined(SVB_PROFILE_MINIMAL)
void WebApp::handleSettings() {
  if (!authorize(true)) return;
  const String deviceName = server_.arg("deviceName");
  const String channelKey = server_.arg("channelKey");
  const String customUrl = server_.arg("customUrl");
  const String adminPassword = server_.arg("adminPassword");
  ReportingBackend backend = ReportingBackend::Svitlobot;
  long interval = 0;
  if (!parseReportingBackend(server_.arg("reportingBackend"), backend) ||
      !validDeviceName(deviceName) ||
      (!channelKey.isEmpty() && !validChannelKey(channelKey)) ||
      ((backend == ReportingBackend::CustomUrl || !customUrl.isEmpty()) &&
       !validCustomUrl(customUrl)) ||
      (!adminPassword.isEmpty() && !validAdminPassword(adminPassword)) ||
      !parseInteger(server_.arg("pingInterval"), kMinimumPingIntervalSeconds,
                    kMaximumPingIntervalSeconds, interval)) {
    sendError(400, F("Invalid settings"));
    return;
  }

  AppConfig candidate = config_;
  const bool hostnameChanged = deviceName != config_.deviceName;
  candidate.reportingBackend = static_cast<uint8_t>(backend);
  copyChecked(candidate.deviceName, sizeof(candidate.deviceName), deviceName);
  copyChecked(candidate.channelKey, sizeof(candidate.channelKey), channelKey);
  copyChecked(candidate.customUrl, sizeof(candidate.customUrl), customUrl);
  if (!adminPassword.isEmpty()) {
    copyChecked(candidate.adminPassword, sizeof(candidate.adminPassword), adminPassword);
  }
  candidate.pingPaused = server_.hasArg("pingPaused");
  candidate.pingIntervalSeconds = static_cast<uint16_t>(interval);
  if (!saveOrError(candidate)) return;
  sendJson(200, F("{\"ok\":true}"));
  api_.requestPingAfter(kPostSavePingDelayMs);
  if (hostnameChanged) scheduleWifiReconfigure();
}
#endif

void WebApp::handleSecurity() {
  if (!authorize(true)) return;
  const String adminPassword = server_.arg("adminPassword");
  const bool clearAdminPassword = server_.hasArg("clearAdminPassword");
  if ((clearAdminPassword && !adminPassword.isEmpty()) ||
      (!clearAdminPassword && !validAdminPassword(adminPassword)) ||
      (!clearAdminPassword && adminPassword.isEmpty())) {
    sendError(400, F("Enter a new admin password or disable panel protection"));
    return;
  }

  AppConfig candidate = config_;
  if (clearAdminPassword) {
    memset(candidate.adminPassword, 0, sizeof(candidate.adminPassword));
  } else {
    copyChecked(candidate.adminPassword, sizeof(candidate.adminPassword), adminPassword);
  }
  if (!saveOrError(candidate)) return;
  sendJson(200, F("{\"ok\":true}"));
}

void WebApp::handleExpert() {
  if (!authorize(true)) return;
#if !SVB_ENABLE_EXPERT
  sendError(404, F("Expert hardware support is not included in this build"));
#else
  const bool enabled = server_.hasArg("enabled");
  const bool offEnabled = server_.hasArg("offEnabled");
  if ((enabled || offEnabled) && !server_.hasArg("ack")) {
    sendError(400, F("Electrical safety acknowledgement is required"));
    return;
  }
  long pin = -1;
  long presentLevel = 1;
  long debounce = 1500;
  if (!parseInteger(server_.arg("pin"), -1, 48, pin) ||
      !parseInteger(server_.arg("presentLevel"), 0, 1, presentLevel) ||
      !parseInteger(server_.arg("debounce"), 100, 30000, debounce)) {
    sendError(400, F("Invalid expert settings"));
    return;
  }
  String reason;
  if (enabled && offEnabled && !PowerMonitor::pinAllowed(pin, reason)) {
    sendJson(400, String(F("{\"error\":\"")) + jsonEscape(reason) + F("\"}"));
    return;
  }

  AppConfig candidate = config_;
  candidate.expert.enabled = enabled;
  candidate.expert.offNotificationEnabled = offEnabled;
  candidate.expert.mainsPin = static_cast<int8_t>(pin);
  candidate.expert.mainsPresentLevel = static_cast<uint8_t>(presentLevel);
  candidate.expert.useInternalPullup = server_.hasArg("pullup");
  candidate.expert.debounceMs = static_cast<uint16_t>(debounce);
  if (!saveOrError(candidate)) return;
  power_.reconfigure();
  sendJson(200, F("{\"ok\":true}"));
#endif
}

void WebApp::handleScan() {
  if (!authorize(true)) return;
  const int count = WiFi.scanNetworks(false, true);
  if (count < 0) {
    WiFi.scanDelete();
    diagnostics_.add(EventLevel::Warning,
                     String(F("Wi-Fi scan failed with code ")) + count);
    sendError(503, F("Wi-Fi scan failed; try again"));
    return;
  }
  String json = F("{\"networks\":[");
  for (int index = 0; index < count; ++index) {
    if (index) json += ',';
    json += F("{\"ssid\":\"");
    json += jsonEscape(WiFi.SSID(index));
    json += F("\",\"rssi\":");
    json += WiFi.RSSI(index);
    json += F(",\"secure\":");
#if defined(ESP8266)
    json += WiFi.encryptionType(index) == ENC_TYPE_NONE ? F("false") : F("true");
#else
    json += WiFi.encryptionType(index) == WIFI_AUTH_OPEN ? F("false") : F("true");
#endif
    json += '}';
  }
  WiFi.scanDelete();
  json += F("]}");
  sendJson(200, json);
}

void WebApp::handleAction() {
  if (!authorize(true)) return;
  const String action = server_.arg("action");
  if (action == F("ping")) {
    api_.requestPingNow();
  } else if (action == F("reconnect")) {
    scheduleWifiReconfigure();
  } else if (action == F("factory-reset") && server_.arg("confirm") == F("RESET")) {
    if (!store_.clear()) {
      sendError(500, F("Cannot erase configuration"));
      return;
    }
    eraseWifiBeforeRestart_ = true;
    restartPending_ = true;
    restartAt_ = millis() + 600;
  } else {
    sendError(400, F("Unknown or unconfirmed action"));
    return;
  }
  sendJson(200, F("{\"ok\":true}"));
}

void WebApp::handleOtaFinished() {
#if SVB_ENABLE_OTA
  if (!otaAuthorized_) {
    sendError(403, F("OTA authorization failed"));
    return;
  }
  if (otaFailed_ || !otaSucceeded_ || Update.hasError()) {
    sendError(500, F("OTA write failed"));
    return;
  }
  sendJson(200, F("{\"ok\":true,\"restarting\":true}"));
  restartPending_ = true;
  restartAt_ = millis() + 700;
#else
  sendError(404, F("OTA is disabled in this build"));
#endif
}

void WebApp::handleOtaUpload() {
#if SVB_ENABLE_OTA
  HTTPUpload& upload = server_.upload();
  if (upload.status == UPLOAD_FILE_START) {
    otaAuthorized_ = authorize(true);
    otaFailed_ = false;
    otaSucceeded_ = false;
    otaMarkerFound_ = false;
    otaMarkerProgress_ = 0;
    if (!otaAuthorized_) return;
#if defined(ESP8266)
    const uint32_t freeSpace = ESP.getFreeSketchSpace();
    if (freeSpace <= 0x1000U ||
        !Update.begin((freeSpace - 0x1000U) & 0xFFFFF000U, U_FLASH)) {
      otaFailed_ = true;
      diagnostics_.add(EventLevel::Error, F("OTA cannot reserve update slot"));
      return;
    }
#else
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
      otaFailed_ = true;
      diagnostics_.add(EventLevel::Error, F("OTA cannot reserve update slot"));
      return;
    }
#endif
    diagnostics_.add(EventLevel::Warning, F("OTA upload started"));
  } else if (upload.status == UPLOAD_FILE_WRITE && otaAuthorized_) {
    inspectOtaChunk(upload.buf, upload.currentSize);
    if (!otaFailed_ && Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      otaFailed_ = true;
      diagnostics_.add(EventLevel::Error, F("OTA flash write failed"));
    }
  } else if (upload.status == UPLOAD_FILE_END && otaAuthorized_) {
    if (!otaFailed_ && !otaMarkerFound_) {
      otaFailed_ = true;
      diagnostics_.add(EventLevel::Error, F("OTA image is for a different SvitloBoard target"));
    }
    if (!otaFailed_) {
      otaSucceeded_ = Update.end(true);
      otaFailed_ = !otaSucceeded_;
      diagnostics_.add(otaSucceeded_ ? EventLevel::Info : EventLevel::Error,
                       otaSucceeded_ ? F("OTA image written") : F("OTA validation failed"));
    } else {
#if defined(ESP8266)
      Update.end();
#else
      Update.abort();
#endif
    }
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
#if defined(ESP8266)
    Update.end();
#else
    Update.abort();
#endif
    otaFailed_ = true;
    otaSucceeded_ = false;
    diagnostics_.add(EventLevel::Warning, F("OTA upload aborted"));
  }
#endif
}

void WebApp::handleNotFound() {
  if (wifi_.apActive()) {
    server_.sendHeader(F("Location"), String(F("http://")) + wifi_.accessPointIp(), true);
    server_.send(302, "text/plain", "");
  } else {
    sendError(404, F("Not found"));
  }
}

bool WebApp::authorize(bool mutation) {
  if (mutation && server_.header("X-Svitloboard-Action") != F("1")) {
    sendError(403, F("Missing action header"));
    return false;
  }
  if (!config_.setupComplete || config_.adminPassword[0] == '\0') return true;
  if (server_.authenticate("admin", config_.adminPassword)) return true;
  server_.requestAuthentication();
  return false;
}

bool WebApp::saveOrError(const AppConfig& candidate) {
  if (!store_.save(candidate)) {
    sendError(500, F("Configuration was not saved; previous version is intact"));
    return false;
  }
  config_ = candidate;
  return true;
}

void WebApp::sendJson(int status, const String& body) {
  server_.sendHeader(F("Cache-Control"), F("no-store"));
  server_.send(status, "application/json; charset=utf-8", body);
}

void WebApp::sendError(int status, const __FlashStringHelper* message) {
  sendJson(status, String(F("{\"error\":\"")) + jsonEscape(String(message)) + F("\"}"));
}

String WebApp::jsonEscape(const String& value) {
  String result;
  result.reserve(value.length() + 8);
  for (size_t index = 0; index < value.length(); ++index) {
    const char c = value[index];
    switch (c) {
      case '\\': result += F("\\\\"); break;
      case '"': result += F("\\\""); break;
      case '\n': result += F("\\n"); break;
      case '\r': result += F("\\r"); break;
      case '\t': result += F("\\t"); break;
      default:
        if (static_cast<uint8_t>(c) < 0x20) result += '?';
        else result += c;
    }
  }
  return result;
}

bool WebApp::parseInteger(const String& value, long minimum, long maximum, long& result) {
  if (value.isEmpty()) return false;
  char* end = nullptr;
  const long parsed = strtol(value.c_str(), &end, 10);
  if (end == value.c_str() || *end != '\0' || parsed < minimum || parsed > maximum) return false;
  result = parsed;
  return true;
}

void WebApp::inspectOtaChunk(const uint8_t* data, size_t length) {
#if SVB_ENABLE_OTA
  if (otaMarkerFound_ || data == nullptr) return;
  constexpr size_t markerLength = sizeof(kOtaCompatibilityMarker) - 1;
  for (size_t index = 0; index < length; ++index) {
    const uint8_t byte = data[index];
    if (byte == static_cast<uint8_t>(kOtaCompatibilityMarker[otaMarkerProgress_])) {
      if (++otaMarkerProgress_ == markerLength) {
        otaMarkerFound_ = true;
        return;
      }
    } else {
      otaMarkerProgress_ =
          byte == static_cast<uint8_t>(kOtaCompatibilityMarker[0]) ? 1 : 0;
    }
  }
#else
  (void)data;
  (void)length;
#endif
}

void WebApp::syncCaptiveDns() {
  if (wifi_.apActive() && !dnsActive_) {
    dnsActive_ = dns_.start(53, "*", WiFi.softAPIP());
  } else if (!wifi_.apActive() && dnsActive_) {
    dns_.stop();
    dnsActive_ = false;
  }
}

void WebApp::scheduleWifiReconfigure() {
  wifiReconfigurePending_ = true;
  // Let the setup response reach captive-portal browsers before changing the
  // ESP radio state. WebKit otherwise reports a misleading "Load failed"
  // even though the configuration has already been committed to flash.
  wifiReconfigureAt_ = millis() + kWifiReconfigureDelayMs;
}

}  // namespace svb
