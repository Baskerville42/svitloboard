#include "PingSlots.h"

#include <assert.h>
#include <stdio.h>

using svb::PingSlots;

constexpr uint32_t kBase = (1700000000U / 540U) * 540U;
static_assert(sizeof(PingSlots) <= 8, "slot state must stay small on ESP8266");

int main() {
  // All boards share the UTC origin, including the last-to-first transition.
  // Repeated loop calls and different HTTP durations never move their phases.
  for (uint8_t count = 2; count <= 4; ++count) {
    PingSlots boards[4];
    const uint32_t period = count * 45U;
    for (uint8_t index = 1; index <= count; ++index) {
      assert(!boards[index - 1].claim(kBase - 1, count, index));
    }
    uint32_t previous = 0;
    unsigned requests = 0;
    for (uint32_t now = kBase; now < kBase + period * 4; ++now) {
      for (uint8_t index = 1; index <= count; ++index) {
        if (!boards[index - 1].claim(now, count, index)) continue;
        assert((now - kBase) % period == (index - 1) * 45U);
        if (previous) assert(now - previous == 45);
        previous = now;
        ++requests;
        assert(!boards[index - 1].claim(now, count, index));
        assert(!boards[index - 1].claim(now + 1, count, index));
      }
    }
    assert(requests == count * 4U);
  }

  PingSlots slot;
  assert(!slot.claim(kBase, 2, 1));  // Startup inside a slot waits for the next.
  assert(!slot.claim(kBase + 1, 2, 1));
  assert(slot.claim(kBase + 91, 2, 1));  // One-second loop jitter is tolerated.
  assert(!slot.claim(kBase + 91, 2, 1));  // A failed send still consumes its slot.
  assert(!slot.claim(kBase + 182, 2, 1));  // A late slot is skipped, not caught up.
  assert(!slot.claim(kBase + 200, 2, 1));
  assert(slot.claim(kBase + 270, 2, 1));

  // NTP may step forward or back: no replay of a consumed UTC slot.
  assert(slot.claim(kBase + 540, 2, 1));
  assert(!slot.claim(kBase + 450, 2, 1));
  assert(!slot.claim(kBase + 540, 2, 1));
  assert(slot.claim(kBase + 630, 2, 1));

  // Reconnect and changed settings also wait for their next full slot.
  slot.reset();
  assert(!slot.claim(kBase + 631, 2, 1));
  assert(slot.claim(kBase + 720, 2, 1));
  assert(!slot.claim(kBase + 765, 2, 2));
  assert(slot.claim(kBase + 855, 2, 2));
  assert(!slot.claim(kBase + 855, 3, 2));
  assert(slot.claim(kBase + 990, 3, 2));

  for (uint8_t count = 0; count <= 5; ++count) {
    for (uint8_t index = 0; index <= 5; ++index) {
      if (count >= 2 && count <= 4 && index >= 1 && index <= count) continue;
      assert(!slot.claim(kBase, count, index));
    }
  }
  puts("Ping slot schedules, jitter, reconnect, missed slots and clock steps passed.");
}
