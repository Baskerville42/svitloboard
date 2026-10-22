#pragma once

#include <Arduino.h>

#include "ConfigStore.h"
#include "Diagnostics.h"

namespace svb {

class BootRecovery {
 public:
  BootRecovery(ConfigStore& store, Diagnostics& diagnostics)
      : store_(store), diagnostics_(diagnostics) {}

  // Returns true when a factory reset was completed and restart was requested.
  bool begin(bool setupComplete);
  void loop();

 private:
  static bool isPowerOnReset();

  ConfigStore& store_;
  Diagnostics& diagnostics_;
  uint32_t stableAt_ = 0;
  uint32_t retryAt_ = 0;
  bool tracking_ = false;
};

}  // namespace svb
