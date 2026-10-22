#pragma once

#include <Arduino.h>

#include "AppConfig.h"
#include "Diagnostics.h"

namespace svb {

class PowerMonitor {
 public:
  enum class Event : uint8_t { None, MainsLost, MainsRestored };

  PowerMonitor(AppConfig& config, Diagnostics& diagnostics)
      : config_(config), diagnostics_(diagnostics) {}

  void begin();
  void reconfigure();
  void loop();
  Event consumeEvent();

  bool available() const { return available_; }
  bool mainsPresent() const { return stableMainsPresent_; }
  String validationError() const { return validationError_; }
  static bool pinAllowed(int pin, String& reason);

 private:
  AppConfig& config_;
  Diagnostics& diagnostics_;
  bool available_ = false;
  bool initialized_ = false;
  bool rawMainsPresent_ = true;
  bool stableMainsPresent_ = true;
  uint32_t rawChangedAt_ = 0;
  Event pendingEvent_ = Event::None;
  String validationError_;
};

}  // namespace svb
