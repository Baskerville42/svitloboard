#pragma once

#include <Arduino.h>

#include "AppConfig.h"
#include "Diagnostics.h"
#include "WifiManager.h"

namespace svb {

class SvitlobotClient {
 public:
  enum class Result : uint8_t {
    Never,
    Success,
    Paused,
    NoKey,
    NoWifi,
    ClockNotReady,
    TlsError,
    HttpError
  };

  SvitlobotClient(AppConfig& config, WifiManager& wifi, Diagnostics& diagnostics)
      : config_(config), wifi_(wifi), diagnostics_(diagnostics) {}

  void begin();
  void loop();
  void requestPingNow();
  void queueOffNotification();
  void cancelQueuedOffNotification();
  String statusPageUrl() const;

  Result result() const { return result_; }
  const char* resultName() const;
  int httpCode() const { return httpCode_; }
  uint32_t lastAttemptMs() const { return lastAttemptMs_; }
  uint32_t lastSuccessMs() const { return lastSuccessMs_; }
  bool offQueued() const { return offQueued_; }
  bool clockReady() const;

 private:
  bool sendRequest(bool off);
  static String urlEncode(const char* value);
  void ensureClockStarted();

  AppConfig& config_;
  WifiManager& wifi_;
  Diagnostics& diagnostics_;
  Result result_ = Result::Never;
  int httpCode_ = 0;
  uint32_t lastAttemptMs_ = 0;
  uint32_t lastSuccessMs_ = 0;
  uint32_t nextPingMs_ = 0;
  uint32_t offRetryAtMs_ = 0;
  uint32_t offRetryMs_ = 30000;
  bool clockStarted_ = false;
  bool pingRequested_ = false;
  bool offQueued_ = false;
};

}  // namespace svb
