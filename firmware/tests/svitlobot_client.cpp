#include <cassert>
#include <ctime>
#include <cstring>
#include <iostream>
#include <vector>

#include "AppConfig.h"
#include "Diagnostics.h"

using std::min;
using namespace svb;
constexpr uint32_t kBase = (1700001000U / 540U) * 540U;
uint32_t hostMs = 1000;
uint32_t hostUnix = kBase;
uint32_t millis() { return hostMs; }
time_t fakeTime(time_t*) { return hostUnix; }
void svb::Diagnostics::add(EventLevel, const String&) {}
bool svb::reportingConfigurationValid(const AppConfig& config) { return config.channelKey[0] != 0; }
bool svb::reportingUsesTls(const AppConfig& config) {
  return config.reportingBackend == 0 || strncmp(config.customUrl, "https://", 8) == 0;
}
struct WifiManager { bool online = true; bool connected() const { return online; } };
struct Attempt { uint32_t ms, utc; bool off; };

// Networking and hardware clocks are the boundary: the tested methods below
// are extracted from production, and all slot decisions use real PingSlots.
class SvitlobotClient {
 public:
  enum class Result { Never, Success, Paused, MissingConfiguration, NoWifi,
                      ClockNotReady, WaitingSlot, TlsError, HttpError };
  AppConfig config_{};
  WifiManager wifi_;
  Diagnostics diagnostics_;
  PingSlots slots_;
  Result result_ = Result::Never;
  uint32_t lastAttemptMs_ = 0, lastSuccessMs_ = 0, nextPingMs_ = 0;
  uint32_t pingRequestAtMs_ = 0, offRetryAtMs_ = 0, offRetryMs_ = 30000;
  bool clockStarted_ = false, pingRequested_ = false, offQueued_ = false;
  bool succeeds = true;
  uint32_t httpDuration = 0;
  unsigned ntpStarts = 0;
  std::vector<Attempt> attempts;
  SvitlobotClient() {
    hostMs = 1000; hostUnix = kBase;
    config_.setupComplete = 1; config_.pingIntervalSeconds = 60;
    config_.pingSlotIndex = 1; config_.channelKey[0] = 'K';
    config_.expert.enabled = config_.expert.offNotificationEnabled = 1;
  }
  void begin(); void loop(); void requestPingNow(); void requestPingAfter(uint32_t delayMs);
  void queueOffNotification(); void cancelQueuedOffNotification();
  bool customBackend() const { return config_.reportingBackend != 0; }
  bool clockReady() const { return hostUnix >= kValidUnixTime; }
  void ensureClockStarted() { if (!clockStarted_) { clockStarted_ = true; ++ntpStarts; } }
  bool sendRequest(bool off) {
    lastAttemptMs_ = millis();
    attempts.push_back({hostMs, hostUnix, off});
    hostMs += httpDuration;
    result_ = succeeds ? Result::Success : Result::HttpError;
    if (succeeds) lastSuccessMs_ = millis();
    return succeeds;
  }
  void at(uint32_t seconds) { hostMs = 1000 + seconds * 1000; hostUnix = kBase + seconds; loop(); }
  void slotted(uint8_t count = 2, uint8_t index = 1) {
    config_.pingSlotCount = count; config_.pingSlotIndex = index; begin();
  }
  void plainHttp() { config_.reportingBackend = 1; strcpy(config_.customUrl, "http://example.test"); }
};
#define time fakeTime
#include "svitlobot_client_production.inc"
#undef time

void disabledMode() {
  SvitlobotClient client;
  client.httpDuration = 5000;
  client.begin(); client.at(0);
  assert(client.attempts.size() == 1 && client.ntpStarts == 1);
  client.at(60); assert(client.attempts.size() == 1);  // Interval starts after HTTP finishes.
  client.at(65); assert(client.attempts.size() == 2);
  client.requestPingNow();
  client.at(94); assert(client.attempts.size() == 2);
  client.at(95); assert(client.attempts.size() == 3);  // Manual request still observes 30 seconds.
  client.config_.pingPaused = 1; client.queueOffNotification(); client.at(96);
  assert(client.attempts.size() == 4 && client.attempts.back().off);  // Legacy OFF remains immediate.

  SvitlobotClient plain;
  plain.plainHttp(); hostUnix = 0; plain.begin(); plain.loop();
  assert(plain.attempts.size() == 1 && plain.ntpStarts == 0);
}

void clockAndStartup() {
  SvitlobotClient client;
  client.slotted(); hostUnix = 0; client.loop();
  assert(client.attempts.empty() && client.ntpStarts == 1);
  assert(client.result_ == SvitlobotClient::Result::ClockNotReady);
  client.at(0); client.at(1); assert(client.attempts.empty());
  client.at(90); assert(client.attempts.size() == 1);
  client.wifi_.online = false; client.at(100);
  client.wifi_.online = true; client.at(180); client.at(181);
  assert(client.attempts.size() == 1);  // Reconnection inside a slot waits for the next full slot.
  client.at(270); assert(client.attempts.size() == 2 && client.ntpStarts == 2);
}

void customIgnoresStoredSlots() {
  SvitlobotClient plain;
  plain.plainHttp(); plain.slotted(4, 4);
  plain.config_.pingIntervalSeconds = 75;
  plain.httpDuration = 5000;
  hostUnix = 0; plain.loop();
  assert(plain.attempts.size() == 1 && plain.ntpStarts == 0);
  plain.at(79); assert(plain.attempts.size() == 1);
  plain.at(80); assert(plain.attempts.size() == 2);
  plain.requestPingNow(); plain.at(109);
  assert(plain.attempts.size() == 2);
  plain.at(110); assert(plain.attempts.size() == 3);

  // Switching back restores the saved Svitlobot slots, with no overdue burst.
  plain.config_.reportingBackend = static_cast<uint8_t>(ReportingBackend::Svitlobot);
  plain.requestPingNow(); plain.at(120); plain.at(140);
  assert(plain.attempts.size() == 3 && plain.ntpStarts == 1);
  plain.at(315); assert(plain.attempts.size() == 4);
  assert(plain.config_.pingSlotCount == 4 && plain.config_.pingSlotIndex == 4);

  SvitlobotClient secure;
  secure.plainHttp(); strcpy(secure.config_.customUrl, "https://example.test");
  secure.slotted(4, 4); hostUnix = 0; secure.loop();
  assert(secure.attempts.empty() && secure.ntpStarts == 1);  // Clock is needed for TLS only.
  secure.at(0); assert(secure.attempts.size() == 1);
  secure.at(60); assert(secure.attempts.size() == 2);  // Normal 60-second cadence, not 180.
}

void manualDelayedAndFailure() {
  SvitlobotClient client;
  client.slotted(); client.at(0); client.at(90);
  client.requestPingNow(); client.at(130); assert(client.attempts.size() == 1);
  client.at(180); assert(client.attempts.size() == 2);
  client.at(200); client.requestPingAfter(80000);
  client.at(270); client.at(280); assert(client.attempts.size() == 2);
  client.succeeds = false; client.at(360); assert(client.attempts.size() == 3);
  client.requestPingNow(); client.at(361); client.at(400);
  assert(client.attempts.size() == 3);  // Failure consumes the slot and cannot be retried between slots.
  client.at(452); client.at(480); assert(client.attempts.size() == 3);  // Missed window is not caught up.
  client.at(540); assert(client.attempts.size() == 4);
}

void offPriorityRetryAndPause() {
  SvitlobotClient client;
  client.slotted(); client.at(0); client.at(90);
  client.config_.pingPaused = 1; client.at(100); client.queueOffNotification();
  client.succeeds = false; client.at(180);
  assert(client.attempts.size() == 2 && client.attempts.back().off);
  client.at(210); client.at(240); assert(client.attempts.size() == 2);
  client.succeeds = true; client.at(270);
  assert(client.attempts.size() == 3 && client.attempts.back().off && !client.offQueued_);
  client.at(360); assert(client.attempts.size() == 3);  // Pause resumes after queued OFF succeeds.
  client.config_.pingPaused = 0; client.at(450);
  assert(client.attempts.size() == 4 && !client.attempts.back().off);
  client.at(460); client.queueOffNotification(); client.cancelQueuedOffNotification();
  client.at(540); assert(!client.attempts.back().off);

  SvitlobotClient plain;
  plain.plainHttp(); plain.slotted(); plain.queueOffNotification();
  assert(!plain.offQueued_);  // Custom backend never supports channelPingOff.
}

void configChangesAndClockSteps() {
  SvitlobotClient client;
  client.slotted(); client.at(0); client.at(90);
  client.config_.pingSlotIndex = 2; client.at(135); client.at(136);
  assert(client.attempts.size() == 1);
  client.at(225); assert(client.attempts.size() == 2);
  client.config_.pingSlotCount = 3; client.at(270);
  client.at(315); assert(client.attempts.size() == 3);
  // A backwards NTP correction must not replay the slot or bypass the monotonic 30-second guard.
  hostMs += 40000; hostUnix = kBase + 180; client.loop();
  assert(client.attempts.size() == 3);
  hostMs += 40000; hostUnix = kBase + 315; client.loop();
  assert(client.attempts.size() == 3);
  hostMs += 40000; hostUnix = kBase + 450; client.loop();
  assert(client.attempts.size() == 4);
}

int main() {
  disabledMode(); clockAndStartup(); customIgnoresStoredSlots(); manualDelayedAndFailure();
  offPriorityRetryAndPause(); configChangesAndClockSteps();
  std::cout << "SvitlobotClient production control flow: defaults, NTP, slots, retries and OFF passed\n";
}
