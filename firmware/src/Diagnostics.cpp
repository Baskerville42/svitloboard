#include "Diagnostics.h"

#include <cstring>

namespace svb {

void Diagnostics::add(EventLevel level, const String& message) {
  DiagnosticEvent& event = events_[head_];
  event.uptimeMs = millis();
  event.level = level;
  const size_t length = min(message.length(), sizeof(event.message) - 1);
  memcpy(event.message, message.c_str(), length);
  event.message[length] = '\0';
  head_ = (head_ + 1) % kCapacity;
  if (count_ < kCapacity) ++count_;

  Serial.print('[');
  Serial.print(eventLevelName(level));
  Serial.print("] ");
  Serial.println(event.message);
}

const DiagnosticEvent& Diagnostics::newest(uint8_t offset) const {
  static const DiagnosticEvent empty{};
  if (offset >= count_) return empty;
  const int index = (static_cast<int>(head_) - 1 - offset + kCapacity) % kCapacity;
  return events_[index];
}

const char* eventLevelName(EventLevel level) {
  switch (level) {
    case EventLevel::Info: return "info";
    case EventLevel::Warning: return "warning";
    case EventLevel::Error: return "error";
  }
  return "unknown";
}

}  // namespace svb
