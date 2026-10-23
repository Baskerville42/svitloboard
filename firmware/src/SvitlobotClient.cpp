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

void SvitlobotClient::begin() {
  nextPingMs_ = millis();
}

void SvitlobotClient::loop() {
  const uint32_t now = millis();
  if (!wifi_.connected()) {
    result_ = Result::NoWifi;
    clockStarted_ = false;
    return;
  }

  ensureClockStarted();
  if (!clockReady()) {
    result_ = Result::ClockNotReady;
    return;
  }

  if (offQueued_) {
    if (!offRetryAtMs_ || deadlineReached(now, offRetryAtMs_)) {
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

  if (config_.channelKey[0] == '\0') {
    result_ = Result::NoKey;
    return;
  }
  if (config_.pingPaused) {
    result_ = Result::Paused;
    return;
  }

  const uint32_t earliestAllowed =
      lastAttemptMs_ + static_cast<uint32_t>(kMinimumPingIntervalSeconds) * 1000UL;
  if ((pingRequested_ || deadlineReached(now, nextPingMs_)) &&
      (lastAttemptMs_ == 0 || deadlineReached(now, earliestAllowed))) {
    pingRequested_ = false;
    sendRequest(false);
    nextPingMs_ = millis() + static_cast<uint32_t>(config_.pingIntervalSeconds) * 1000UL;
  }
}

void SvitlobotClient::requestPingNow() {
  pingRequested_ = true;
}

void SvitlobotClient::queueOffNotification() {
  if (config_.expert.enabled && config_.expert.offNotificationEnabled) {
    offQueued_ = true;
    offRetryAtMs_ = 0;
    offRetryMs_ = 30000;
    diagnostics_.add(EventLevel::Warning, F("Immediate OFF notification queued"));
  }
}

void SvitlobotClient::cancelQueuedOffNotification() {
  if (!offQueued_) return;
  offQueued_ = false;
  offRetryAtMs_ = 0;
  offRetryMs_ = 30000;
  diagnostics_.add(EventLevel::Info, F("Stale OFF notification cancelled after mains recovery"));
}

const char* SvitlobotClient::resultName() const {
  switch (result_) {
    case Result::Never: return "never";
    case Result::Success: return "success";
    case Result::Paused: return "paused";
    case Result::NoKey: return "no_key";
    case Result::NoWifi: return "no_wifi";
    case Result::ClockNotReady: return "clock_not_ready";
    case Result::TlsError: return "tls_error";
    case Result::HttpError: return "http_error";
  }
  return "unknown";
}

bool SvitlobotClient::clockReady() const {
  return time(nullptr) >= static_cast<time_t>(kValidUnixTime);
}

bool SvitlobotClient::sendRequest(bool off) {
  if (!wifi_.connected() || config_.channelKey[0] == '\0') return false;
  lastAttemptMs_ = millis();
  httpCode_ = 0;

  const String path = off ? F("/channelPingOff?channel_key=") : F("/channelPing?channel_key=");
  const String url = String(F("https://")) + kApiHost + path + urlEncode(config_.channelKey);

#if defined(ESP8266)
  BearSSL::WiFiClientSecure secureClient;
  // Parsing a PEM trust anchor allocates memory. Keep one parsed copy for the
  // device lifetime instead of allocating and freeing it on every heartbeat.
  static BearSSL::X509List trustAnchor(kGoogleTrustServicesRootR4);
  secureClient.setTrustAnchors(&trustAnchor);
  secureClient.setTimeout(kHttpTimeoutMs);
#else
  WiFiClientSecure secureClient;
  secureClient.setCACert(kGoogleTrustServicesRootR4);
  // ESP32's secure-client timeout API uses seconds, unlike HTTPClient and
  // ESP8266's stream timeout, which use milliseconds.
  constexpr uint32_t secureTimeoutSeconds = (kHttpTimeoutMs + 999U) / 1000U;
  secureClient.setHandshakeTimeout(secureTimeoutSeconds);
  secureClient.setTimeout(secureTimeoutSeconds);
#endif

  HTTPClient http;
#if !defined(ESP8266)
  http.setConnectTimeout(kHttpTimeoutMs);
#endif
  http.setTimeout(kHttpTimeoutMs);
  http.useHTTP10(true);
  if (!http.begin(secureClient, url)) {
    result_ = Result::TlsError;
    diagnostics_.add(EventLevel::Error, F("Cannot initialize HTTPS request"));
    return false;
  }
  http.addHeader(F("User-Agent"), String(F("SvitloBoard/")) + kFirmwareVersion);
  httpCode_ = http.GET();
  http.end();

  if (httpCode_ >= 200 && httpCode_ < 300) {
    result_ = Result::Success;
    lastSuccessMs_ = millis();
    diagnostics_.add(EventLevel::Info, off ? F("OFF notification accepted")
                                           : F("Svitlobot ping accepted"));
    return true;
  }

  result_ = httpCode_ < 0 ? Result::TlsError : Result::HttpError;
  diagnostics_.add(EventLevel::Warning,
                   String(off ? F("OFF request failed: ") : F("Ping failed: ")) + httpCode_);
  return false;
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
