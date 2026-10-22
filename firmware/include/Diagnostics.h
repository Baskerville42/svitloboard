#pragma once

#include <Arduino.h>

namespace svb {

enum class EventLevel : uint8_t { Info, Warning, Error };

struct DiagnosticEvent {
  uint32_t uptimeMs;
  EventLevel level;
  char message[72];
};

class Diagnostics {
 public:
  static constexpr uint8_t kCapacity = 16;

  void add(EventLevel level, const String& message);
  uint8_t count() const { return count_; }
  const DiagnosticEvent& newest(uint8_t offset) const;

 private:
  DiagnosticEvent events_[kCapacity]{};
  uint8_t head_ = 0;
  uint8_t count_ = 0;
};

const char* eventLevelName(EventLevel level);

}  // namespace svb
