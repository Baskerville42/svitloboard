#include "SvitlobotClient.h"

#include <time.h>

#if defined(ESP8266)
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#else
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#endif

#include "BuildConfig.h"
#include "RootCa.h"

namespace svb {
namespace {

int performGet(WiFiClient& client, const String& url, bool& started) {
  HTTPClient http;
#if !defined(ESP8266)
  http.setConnectTimeout(kHttpTimeoutMs);
#endif
  http.setTimeout(kHttpTimeoutMs);
  http.useHTTP10(true);
  started = http.begin(client, url);
  if (!started) return 0;
  http.addHeader(F("User-Agent"), String(F("SvitloBoard/")) + kFirmwareVersion);
  const int code = http.GET();
  http.end();
  return code;
}

}  // namespace

void SvitlobotClient::begin() {
  nextPingMs_ = millis();
}

void SvitlobotClient::loop() {
  const uint32_t now = millis();
  if (!config_.setupComplete) {
    result_ = Result::Never;
    clockStarted_ = false;
    slots_.reset();
    return;
  }
  if (!wifi_.connected()) {
    result_ = Result::NoWifi;
    clockStarted_ = false;
    slots_.reset();
    return;
  }

  if (!reportingConfigurationValid(config_)) {
    result_ = Result::MissingConfiguration;
    return;
  }

  const bool slotted =
      config_.reportingBackend == static_cast<uint8_t>(ReportingBackend::Svitlobot) &&
      config_.pingSlotCount != 0;
  if (!slotted) slots_.reset();
  if (reportingUsesTls(config_)) {
    ensureClockStarted();
    if (!clockReady()) {
      result_ = Result::ClockNotReady;
      slots_.reset();
      return;
    }
  }

  const bool minimumDue = lastAttemptMs_ == 0 ||
      deadlineReached(now, lastAttemptMs_ +
          static_cast<uint32_t>(kMinimumPingIntervalSeconds) * 1000UL);
  if (slotted && (result_ == Result::Never || result_ == Result::NoWifi ||
                  result_ == Result::ClockNotReady || result_ == Result::MissingConfiguration)) {
    result_ = Result::WaitingSlot;
  }

  if (offQueued_ && customBackend()) {
    cancelQueuedOffNotification();
    diagnostics_.add(EventLevel::Info,
                     F("OFF notification is not configured for the custom URL backend"));
  }

  if (offQueued_) {
    if ((!offRetryAtMs_ || deadlineReached(now, offRetryAtMs_)) &&
        (!slotted || (minimumDue && slots_.claim(
            static_cast<uint32_t>(time(nullptr)), config_.pingSlotCount, config_.pingSlotIndex)))) {
      if (sendRequest(true)) {
        offQueued_ = false;
        offRetryAtMs_ = 0;
        offRetryMs_ = 30000;
      } else {
        offRetryAtMs_ = millis() + offRetryMs_;
        offRetryMs_ = min<uint32_t>(offRetryMs_ * 2U, 300000U);
      }
    }
    return;
  }

  if (config_.pingPaused) {
    result_ = Result::Paused;
    return;
  }

  const bool requestedPingDue = pingRequested_ && deadlineReached(now, pingRequestAtMs_);
  const bool pingDue = slotted ? (!pingRequested_ || requestedPingDue) :
      (requestedPingDue || (!pingRequested_ && deadlineReached(now, nextPingMs_)));
  if (pingDue && minimumDue &&
      (!slotted || slots_.claim(static_cast<uint32_t>(time(nullptr)),
                               config_.pingSlotCount, config_.pingSlotIndex))) {
    pingRequested_ = false;
    sendRequest(false);
    nextPingMs_ = millis() + static_cast<uint32_t>(config_.pingIntervalSeconds) * 1000UL;
  }
}

void SvitlobotClient::requestPingNow() {
  requestPingAfter(0);
}

void SvitlobotClient::requestPingAfter(uint32_t delayMs) {
  pingRequested_ = true;
  pingRequestAtMs_ = millis() + delayMs;
  if (delayMs > 0) nextPingMs_ = pingRequestAtMs_;
}

void SvitlobotClient::queueOffNotification() {
  if (config_.expert.enabled && config_.expert.offNotificationEnabled && !customBackend()) {
    offQueued_ = true;
    offRetryAtMs_ = 0;
    offRetryMs_ = 30000;
    diagnostics_.add(EventLevel::Warning, F("OFF notification queued"));
  }
}

void SvitlobotClient::cancelQueuedOffNotification() {
  if (!offQueued_) return;
  offQueued_ = false;
  offRetryAtMs_ = 0;
  offRetryMs_ = 30000;
  diagnostics_.add(EventLevel::Info, F("Stale OFF notification cancelled after mains recovery"));
}

String SvitlobotClient::statusPageUrl() const {
  if (customBackend()) return String();
  return String(F("https://")) + kApiHost + F("/status?channel_key=") +
         urlEncode(config_.channelKey);
}

const char* SvitlobotClient::resultName() const {
  switch (result_) {
    case Result::Never: return "never";
    case Result::Success: return "success";
    case Result::Paused: return "paused";
    case Result::MissingConfiguration: return "missing_configuration";
    case Result::NoWifi: return "no_wifi";
    case Result::ClockNotReady: return "clock_not_ready";
    case Result::WaitingSlot: return "waiting_slot";
    case Result::TlsError: return "tls_error";
    case Result::HttpError: return "http_error";
  }
  return "unknown";
}

bool SvitlobotClient::clockReady() const {
  return time(nullptr) >= static_cast<time_t>(kValidUnixTime);
}

bool SvitlobotClient::sendRequest(bool off) {
  if (!wifi_.connected() || !reportingConfigurationValid(config_) ||
      (off && customBackend())) {
    return false;
  }
  lastAttemptMs_ = millis();
  httpCode_ = 0;

  String url;
  if (customBackend()) {
    url = config_.customUrl;
  } else {
    const String path =
        off ? F("/channelPingOff?channel_key=") : F("/channelPing?channel_key=");
    url = String(F("https://")) + kApiHost + path + urlEncode(config_.channelKey);
  }
  const bool tls = url.startsWith("https://");

#if defined(ESP8266)
  bool started = false;
  if (tls) {
    BearSSL::WiFiClientSecure secureClient;
    // Parsing a PEM trust anchor allocates memory. Keep one parsed copy for the
    // device lifetime instead of allocating and freeing it on every heartbeat.
    static BearSSL::X509List trustAnchor(kGoogleTrustServicesRootR4);
    secureClient.setTrustAnchors(&trustAnchor);
    secureClient.setTimeout(kHttpTimeoutMs);
    httpCode_ = performGet(secureClient, url, started);
  } else {
    WiFiClient plainClient;
    httpCode_ = performGet(plainClient, url, started);
  }
#else
  bool started = false;
  if (tls) {
    WiFiClientSecure secureClient;
    secureClient.setCACert(kGoogleTrustServicesRootR4);
    // ESP32's secure-client timeout API uses seconds, unlike HTTPClient and
    // ESP8266's stream timeout, which use milliseconds.
    constexpr uint32_t secureTimeoutSeconds = (kHttpTimeoutMs + 999U) / 1000U;
    secureClient.setHandshakeTimeout(secureTimeoutSeconds);
    secureClient.setTimeout(secureTimeoutSeconds);
    httpCode_ = performGet(secureClient, url, started);
  } else {
    WiFiClient plainClient;
    httpCode_ = performGet(plainClient, url, started);
  }
#endif
  if (!started) {
    result_ = tls ? Result::TlsError : Result::HttpError;
    diagnostics_.add(EventLevel::Error,
                     tls ? F("Cannot initialize HTTPS request")
                         : F("Cannot initialize HTTP request"));
    return false;
  }

  if (httpCode_ >= 200 && httpCode_ < 300) {
    result_ = Result::Success;
    lastSuccessMs_ = millis();
    diagnostics_.add(EventLevel::Info, off ? F("OFF notification accepted")
                                           : F("Heartbeat accepted"));
    return true;
  }

  result_ = httpCode_ < 0 && tls ? Result::TlsError : Result::HttpError;
  String failure = String(off ? F("OFF request failed: ") : F("Ping failed: ")) + httpCode_;
  diagnostics_.add(EventLevel::Warning, failure);
  return false;
}

bool SvitlobotClient::customBackend() const {
  return config_.reportingBackend == static_cast<uint8_t>(ReportingBackend::CustomUrl);
}

String SvitlobotClient::urlEncode(const char* value) {
  static const char hex[] = "0123456789ABCDEF";
  String result;
  result.reserve(strlen(value) * 3);
  for (const uint8_t* cursor = reinterpret_cast<const uint8_t*>(value); *cursor; ++cursor) {
    const uint8_t c = *cursor;
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      result += static_cast<char>(c);
    } else {
      result += '%';
      result += hex[c >> 4];
      result += hex[c & 0x0F];
    }
  }
  return result;
}

void SvitlobotClient::ensureClockStarted() {
  if (clockStarted_) return;
  configTime(0, 0, "time.cloudflare.com", "time.google.com", "pool.ntp.org");
  clockStarted_ = true;
  diagnostics_.add(EventLevel::Info, F("NTP synchronization started"));
}

}  // namespace svb
