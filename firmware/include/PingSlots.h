#pragma once

#include <stdint.h>

namespace svb {

constexpr uint8_t kMaximumPingSlotCount = 4;
constexpr uint16_t kPingSlotSpacingSeconds = 45;
constexpr uint8_t kPingSlotWindowSeconds = 2;

// UTC Unix seconds are the shared origin: boot time and HTTP duration do not
// move a board's slot. No heap, timers, or persistent writes are needed.
class PingSlots {
 public:
  void reset() { count_ = 0; }

  bool claim(uint32_t unixSeconds, uint8_t count, uint8_t index) {
    if (count < 2 || count > kMaximumPingSlotCount || index < 1 || index > count) {
      reset();
      return false;
    }
    const uint16_t period = count * kPingSlotSpacingSeconds;
    const uint16_t offset = (index - 1) * kPingSlotSpacingSeconds;
    const uint32_t age = (unixSeconds - offset) % period;
    const uint32_t slot = unixSeconds - age;
    if (count_ != count || index_ != index) {
      count_ = count;
      index_ = index;
      lastSlot_ = slot;
      return false;  // Start/reconfigure/reconnect at the next full slot.
    }
    if (age >= kPingSlotWindowSeconds || slot <= lastSlot_) return false;
    lastSlot_ = slot;  // Consume even a failed request; never catch up a slot.
    return true;
  }

 private:
  uint32_t lastSlot_ = 0;
  uint8_t count_ = 0;
  uint8_t index_ = 0;
};

}  // namespace svb
