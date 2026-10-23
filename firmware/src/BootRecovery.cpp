#include "BootRecovery.h"

#if defined(ESP8266)
#include <ESP8266WiFi.h>
extern "C" {
#include <user_interface.h>
}
#else
#include <WiFi.h>
#include <esp_system.h>
#endif

#include "BuildConfig.h"

namespace svb {

bool BootRecovery::begin(bool setupComplete) {
  if (!setupComplete) {
    store_.discardFastBootCounter();
    return false;
  }
  if (!isPowerOnReset()) return false;

  uint8_t count = 0;
  if (!store_.recordFastBoot(count)) {
    diagnostics_.add(EventLevel::Error, F("Cannot update fast-boot recovery counter"));
    return false;
  }
  diagnostics_.add(EventLevel::Info, String(F("Fast power-cycle count: ")) + count);

  if (count >= kFastBootResetThreshold) {
    if (!store_.clear()) {
      diagnostics_.add(EventLevel::Error, F("Fast power-cycle factory reset failed"));
      tracking_ = true;
      stableAt_ = millis() + kFastBootWindowMs;
      return false;
    }
    diagnostics_.add(EventLevel::Warning, F("Factory reset after four fast power cycles"));
    WiFi.disconnect(true, true);
    delay(100);
    ESP.restart();
    return true;
  }

  tracking_ = true;
  stableAt_ = millis() + kFastBootWindowMs;
  return false;
}

void BootRecovery::loop() {
  if (!tracking_) return;
  const uint32_t now = millis();
  if (!deadlineReached(now, stableAt_) || (retryAt_ && !deadlineReached(now, retryAt_))) return;
  if (store_.markFastBootStable()) {
    tracking_ = false;
    diagnostics_.add(EventLevel::Info, F("Fast power-cycle counter cleared"));
  } else {
    retryAt_ = now + 1000;
    diagnostics_.add(EventLevel::Warning, F("Retrying fast-boot counter clear"));
  }
}

bool BootRecovery::isPowerOnReset() {
#if defined(ESP8266)
  const rst_info* resetInfo = ESP.getResetInfoPtr();
  return resetInfo != nullptr && resetInfo->reason == REASON_DEFAULT_RST;
#else
  return esp_reset_reason() == ESP_RST_POWERON;
#endif
}

}  // namespace svb
