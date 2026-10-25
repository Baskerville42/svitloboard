#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>

#include "PingSlots.h"

// Only the server argument and Arduino String surfaces used by the production
// parser are stubbed. The Python runner extracts both production method bodies.
class String : public std::string {
 public:
  using std::string::string;
  bool isEmpty() const { return empty(); }
};

enum class ReportingBackend : uint8_t { Svitlobot = 0, CustomUrl = 1 };

struct AppConfig {
  uint8_t pingSlotCount = 3;
  uint8_t pingSlotIndex = 2;
};

struct Server {
  std::map<std::string, String> fields;
  bool hasArg(const char* key) const { return fields.count(key) != 0; }
  String arg(const char* key) const {
    const auto found = fields.find(key);
    return found == fields.end() ? String() : found->second;
  }
};

using svb::kMaximumPingSlotCount;

class WebApp {
 public:
  Server server_;
  bool parsePingSlots(AppConfig& candidate, ReportingBackend backend);
  static bool parseInteger(const String& value, long minimum, long maximum, long& result);
};

#include "slot_api_production.inc"

int main() {
  WebApp app;
  AppConfig config;

  // Older clients submit no slot fields and must preserve an enabled schedule.
  assert(app.parsePingSlots(config, ReportingBackend::Svitlobot));
  assert(config.pingSlotCount == 3 && config.pingSlotIndex == 2);

  // Unchecked HTML checkboxes are omitted; the selectors still explicitly
  // disable the mode. Ignore their values and normalize the disabled state.
  app.server_.fields = {{"pingSlotCount", "4"}, {"pingSlotIndex", "3"}};
  assert(app.parsePingSlots(config, ReportingBackend::Svitlobot));
  assert(config.pingSlotCount == 0 && config.pingSlotIndex == 1);
  app.server_.fields = {{"pingSlotCount", "invalid"}};
  assert(app.parsePingSlots(config, ReportingBackend::Svitlobot));
  assert(config.pingSlotCount == 0 && config.pingSlotIndex == 1);
  app.server_.fields.clear();
  assert(app.parsePingSlots(config, ReportingBackend::Svitlobot));
  assert(config.pingSlotCount == 0 && config.pingSlotIndex == 1);

  for (int count = 2; count <= svb::kMaximumPingSlotCount; ++count) {
    for (int index = 1; index <= count; ++index) {
      app.server_.fields = {{"pingSlotEnabled", "on"},
                            {"pingSlotCount", String(1, static_cast<char>('0' + count))},
                            {"pingSlotIndex", String(1, static_cast<char>('0' + index))}};
      assert(app.parsePingSlots(config, ReportingBackend::Svitlobot));
      assert(config.pingSlotCount == count && config.pingSlotIndex == index);
    }
  }

  // A rejected request must not partially replace an existing configuration.
  for (const char* bad : {"", "0", "1", "5", "-2", "2.5", "2x", "abc",
                          "99999999999999999999"}) {
    config = AppConfig{};
    app.server_.fields = {{"pingSlotEnabled", "on"}, {"pingSlotCount", bad},
                          {"pingSlotIndex", "1"}};
    assert(!app.parsePingSlots(config, ReportingBackend::Svitlobot));
    assert(config.pingSlotCount == 3 && config.pingSlotIndex == 2);
  }
  for (const char* bad : {"", "0", "4", "-1", "1.5", "1x", "abc",
                          "99999999999999999999"}) {
    config = AppConfig{};
    app.server_.fields = {{"pingSlotEnabled", "on"}, {"pingSlotCount", "3"},
                          {"pingSlotIndex", bad}};
    assert(!app.parsePingSlots(config, ReportingBackend::Svitlobot));
    assert(config.pingSlotCount == 3 && config.pingSlotIndex == 2);
  }
  for (const char* missing : {"pingSlotCount", "pingSlotIndex"}) {
    app.server_.fields = {{"pingSlotEnabled", "on"}, {"pingSlotCount", "3"},
                          {"pingSlotIndex", "2"}};
    app.server_.fields.erase(missing);
    assert(!app.parsePingSlots(config, ReportingBackend::Svitlobot));
    assert(config.pingSlotCount == 3 && config.pingSlotIndex == 2);
  }
  app.server_.fields = {{"pingSlotEnabled", "on"}};
  assert(!app.parsePingSlots(config, ReportingBackend::Svitlobot));
  // Slot fields are exclusively Svitlobot preferences. A Custom URL (or a
  // future backend) must ignore even malformed fields and retain those values.
  for (const ReportingBackend backend : {ReportingBackend::CustomUrl,
                                        static_cast<ReportingBackend>(255)}) {
    for (const auto& fields : std::initializer_list<std::map<std::string, String>>{
             {}, {{"pingSlotCount", "2"}, {"pingSlotIndex", "1"}},
             {{"pingSlotEnabled", "on"}, {"pingSlotCount", "4"}, {"pingSlotIndex", "4"}},
             {{"pingSlotEnabled", "on"}, {"pingSlotCount", "bad"}, {"pingSlotIndex", "99"}},
             {{"pingSlotEnabled", "on"}}}) {
      config = AppConfig{};
      app.server_.fields = fields;
      assert(app.parsePingSlots(config, backend));
      assert(config.pingSlotCount == 3 && config.pingSlotIndex == 2);
    }
  }
  std::cout << "Slot POST parser: valid, invalid, missing, legacy, and backend-isolated requests passed\n";
}
