#include "PowerMonitor.h"

#include "BuildConfig.h"

namespace svb {

void PowerMonitor::begin() {
  reconfigure();
}

void PowerMonitor::reconfigure() {
  available_ = false;
  initialized_ = false;
  pendingEvent_ = Event::None;
  validationError_ = String();

#if !SVB_ENABLE_EXPERT
  validationError_ = F("Expert hardware support is not included in this build");
  return;
#else
  if (!config_.expert.enabled || !config_.expert.offNotificationEnabled) return;
  if (!pinAllowed(config_.expert.mainsPin, validationError_)) {
    diagnostics_.add(EventLevel::Error, String(F("Power input disabled: ")) + validationError_);
    return;
  }

  pinMode(config_.expert.mainsPin, config_.expert.useInternalPullup ? INPUT_PULLUP : INPUT);
  delay(2);
  rawMainsPresent_ = digitalRead(config_.expert.mainsPin) == config_.expert.mainsPresentLevel;
  // Treat an initially absent input as a transition from the safe/default
  // "present" state. loop() will emit MainsLost only after the configured
  // debounce, which covers a board that boots from backup power after mains
  // has already disappeared without trusting a single startup sample.
  stableMainsPresent_ = true;
  rawChangedAt_ = millis();
  initialized_ = true;
  available_ = true;
  diagnostics_.add(EventLevel::Info,
                   String(F("Isolated mains input active on GPIO ")) + config_.expert.mainsPin);
#endif
}

void PowerMonitor::loop() {
  if (!available_ || !initialized_) return;
  const uint32_t now = millis();
  const bool observed = digitalRead(config_.expert.mainsPin) == config_.expert.mainsPresentLevel;
  if (observed != rawMainsPresent_) {
    rawMainsPresent_ = observed;
    rawChangedAt_ = now;
    return;
  }
  if (observed == stableMainsPresent_ ||
      !deadlineReached(now, rawChangedAt_ + config_.expert.debounceMs)) {
    return;
  }

  stableMainsPresent_ = observed;
  pendingEvent_ = observed ? Event::MainsRestored : Event::MainsLost;
  diagnostics_.add(observed ? EventLevel::Info : EventLevel::Warning,
                   observed ? F("Mains power restored") : F("Mains power lost"));
}

PowerMonitor::Event PowerMonitor::consumeEvent() {
  const Event result = pendingEvent_;
  pendingEvent_ = Event::None;
  return result;
}

bool PowerMonitor::pinAllowed(int pin, String& reason) {
#ifdef SVB_RELAY_GPIO
  if (pin == SVB_RELAY_GPIO) {
    reason = F("GPIO is reserved for the onboard relay and cannot be used as an input");
    return false;
  }
#endif
#if defined(ESP8266)
#ifdef SVB_RELAY_GPIO
  // Basic R2/R3 reserve GPIO12 for the relay and GPIO13 for the status LED;
  // GPIO0 is the button/boot strap. GPIO14 is the only conservative sensor
  // input shared by the supported Sonoff targets.
  const bool allowed = pin == 14;
  if (!allowed) {
    reason = F("Sonoff Basic allows the isolated input only on GPIO 14");
  }
  return allowed;
#else
  // GPIO0/2/15 are boot straps, 1/3 are UART and 6-11 are connected to flash.
  const bool allowed = pin == 4 || pin == 5 || pin == 12 || pin == 13 || pin == 14;
  if (!allowed) {
    reason = F("ESP8266 supports safe digital input only on GPIO 4, 5, 12, 13 or 14");
  }
  return allowed;
#endif
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
  // DevKitM-1: avoid GPIO2/8/9 straps, GPIO18/19 native USB, GPIO20/21
  // console UART, and GPIO12-17 used by integrated flash.
  const bool allowed = pin == 0 || pin == 1 || pin == 3 || pin == 4 ||
                       pin == 5 || pin == 6 || pin == 7 || pin == 10;
  if (!allowed) {
    reason = F("ESP32-C3 allows this input only on GPIO 0, 1, 3-7 or 10");
  }
  return allowed;
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
  // DevKitC-1 N8: avoid boot straps, native USB, console UART, RGB LED, and
  // the GPIO26-32 flash/PSRAM bus. Keep the list conservative across revisions.
  const bool allowed = (pin >= 4 && pin <= 18) || pin == 21 ||
                       (pin >= 33 && pin <= 42) || pin == 47;
  if (!allowed) {
    reason = F("ESP32-S3 allows this input only on GPIO 4-18, 21, 33-42 or 47");
  }
  return allowed;
#else
  // Original ESP32: avoid straps, UART0, integrated-flash pins, and input-only
  // pins without internal pull resistors. This matches ESP32-WROOM-32 DevKit.
  const bool allowed = pin == 13 || pin == 14 || pin == 16 || pin == 17 ||
                       pin == 18 || pin == 19 || pin == 21 || pin == 22 ||
                       pin == 23 || (pin >= 25 && pin <= 27) ||
                       pin == 32 || pin == 33;
  if (!allowed) {
    reason = F("ESP32 DevKit allows this input only on GPIO 13, 14, 16-19, 21-23, 25-27, 32 or 33");
  }
  return allowed;
#endif
}

}  // namespace svb
