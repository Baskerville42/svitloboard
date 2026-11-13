#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <functional>
#include <vector>

#include <WiFi.h>
#include "SvitlobotClient.h"

namespace {

constexpr uint32_t kBaseUnix = (1700001000U / 540U) * 540U;
uint32_t nowMs = 100;
uint32_t ntpStartedAt = 0;
unsigned ntpStarts = 0;
unsigned checks = 0;

struct Request {
  uint32_t atMs;
  uint32_t unixSeconds;
  String url;
};
std::vector<Request> requests;

void require(bool value, const char* message) {
  ++checks;
  if (!value) {
    std::fprintf(stderr, "FAIL at %u ms: %s\n", nowMs, message);
    std::exit(1);
  }
}

bool clockSynchronized() {
  return ntpStartedAt && nowMs - ntpStartedAt >= 2000;
}

struct Device {
  svb::AppConfig config{};
  svb::Diagnostics diagnostics;
  svb::WifiManager wifi{config, diagnostics};
  svb::SvitlobotClient reporting{config, wifi, diagnostics};

  explicit Device(bool slotted = false) {
    nowMs = 100;
    ntpStartedAt = ntpStarts = 0;
    requests.clear();
    WiFi = HostWiFi{};
    svb::setConfigDefaults(config);
    config.setupComplete = 1;
    config.wifi[0].enabled = 1;
    std::strcpy(config.wifi[0].ssid, "home");
    std::strcpy(config.wifi[0].password, "password");
    std::strcpy(config.channelKey, "K");
    if (slotted) { config.pingSlotCount = 3; config.pingSlotIndex = 2; }
    wifi.begin();
    reporting.begin();
  }

  void tick() { wifi.loop(); reporting.loop(); }
  void runFor(uint32_t duration) {
    const uint32_t start = nowMs;
    while (nowMs - start < duration) {
      nowMs += min<uint32_t>(100, duration - (nowMs - start));
      tick();
    }
  }
  void waitUntil(const std::function<bool()>& done, uint32_t timeout) {
    const uint32_t start = nowMs;
    while (!done() && nowMs - start < timeout) runFor(100);
    require(done(), "automatic recovery must finish within its bounded retry/slot window");
  }
};

constexpr uint32_t kRecoveryBound = svb::kWifiMaximumBackoffMs +
    svb::kWifiProfileCount * svb::kWifiAttemptMs + 30000;

void delayedRouter(uint32_t unavailableMs, bool slotted, bool slowDhcp = false) {
  Device device(slotted);
  if (slowDhcp) { WiFi.associationMs = 3000; WiFi.dhcpMs = 20000; }
  const auto original = device.config;
  device.runFor(unavailableMs);
  require(device.wifi.apActive() && WiFi.ap, "absent router exposes recovery AP");
  require(WiFi.currentMode == WIFI_AP_STA, "recovery AP must retain a station interface");
  require(device.reporting.result() == svb::SvitlobotClient::Result::NoWifi,
          "reporting waits for Wi-Fi throughout the outage");
  require(requests.empty() && ntpStarts == 0, "no requests or NTP before station DHCP");
  require(std::memcmp(&original, &device.config, sizeof(original)) == 0,
          "temporary absence preserves the saved configuration");
  const size_t beforeAttempts = WiFi.attempts.size();
  require(beforeAttempts > 1, "attempts repeat even after the recovery AP is active");
  for (size_t index = 1; index < beforeAttempts; ++index) {
    require(WiFi.attempts[index].mode == WIFI_AP_STA,
            "every recovery attempt keeps the AP available");
  }

  const uint32_t routerAt = nowMs;
  WiFi.routerAvailable = true;
  device.waitUntil([&device] { return device.wifi.connected(); }, kRecoveryBound);
  require(WiFi.associations == 1 && WiFi.leases == 1, "real manager reaches association and DHCP");
  require(device.wifi.state() == svb::WifiManager::State::Connected,
          "manager observes the DHCP event");
  require(device.reporting.result() == svb::SvitlobotClient::Result::ClockNotReady,
          "HTTPS waits for asynchronous NTP after DHCP");
  require(ntpStarts == 1 && requests.empty(), "first station recovery starts NTP without a premature ping");
  require(device.wifi.apActive(), "AP remains available during the successful handoff");
  const uint32_t connectedAt = nowMs;
  const uint32_t slotAllowance = slotted ? 135000 : 0;
  device.waitUntil([] { return !requests.empty(); }, 3000 + slotAllowance);
  require(device.reporting.result() == svb::SvitlobotClient::Result::Success &&
              device.reporting.httpCode() == 200,
          "automatic heartbeat reaches HTTP success through production request code");
  require(requests.front().atMs >= connectedAt + 2000, "heartbeat follows NTP completion");
  if (slotted) {
    require(requests.front().unixSeconds % 135 == 45, "recovered board waits for its next assigned slot");
  }
  if (nowMs - connectedAt < svb::kProvisioningApGraceMs) {
    device.runFor(svb::kProvisioningApGraceMs - (nowMs - connectedAt));
  }
  require(!device.wifi.apActive() && !WiFi.ap && WiFi.currentMode == WIFI_STA,
          "successful recovery closes the AP after its grace period");
  require(std::memcmp(&original, &device.config, sizeof(original)) == 0,
          "completed recovery keeps the original credentials and reporting settings");
  std::printf("Router after %u s%s%s: DHCP +%u ms, first heartbeat +%u ms, AP handoff OK\n",
              unavailableMs / 1000, slotted ? " (slotted)" : "", slowDhcp ? " (slow DHCP)" : "",
              connectedAt - routerAt, requests.front().atMs - routerAt);

  // A later router reboot must traverse the same Wi-Fi -> time -> reporting
  // path without a manual reconnect, reconfigure, or ping call.
  WiFi.routerAvailable = false;
  const size_t beforeRequests = requests.size();
  device.runFor(90000);
  require(device.wifi.apActive() && requests.size() == beforeRequests,
          "another outage reopens AP and suspends heartbeat requests");
  WiFi.routerAvailable = true;
  device.waitUntil([beforeRequests] { return requests.size() > beforeRequests; },
                   kRecoveryBound + slotAllowance);
  require(WiFi.leases == 2 && ntpStarts == 2,
          "a second recovery obtains a fresh lease and restarts time synchronization");
  require(device.reporting.lastSuccessMs() == requests.back().atMs,
          "reporting records the resumed automatic heartbeat");
}

}  // namespace

HostEsp ESP;
HostWiFi WiFi;
uint32_t millis() { return nowMs; }

extern "C" time_t time(time_t* output) noexcept(noexcept(::time(nullptr))) {
  const time_t current = clockSynchronized() ? kBaseUnix + nowMs / 1000 : 0;
  if (output) *output = current;
  return current;
}

void configTime(long, int, const char*, const char*, const char*) {
  require(WiFi.status() == WL_CONNECTED, "NTP starts only after a station lease is available");
  ++ntpStarts;
  ntpStartedAt = nowMs;
}

int hostGet(const String& url, bool secure, bool trusted, bool timeoutSet) {
  require(WiFi.status() == WL_CONNECTED && clockSynchronized(),
          "HTTP request requires restored Wi-Fi and a synchronized clock");
  require(secure && trusted && timeoutSet, "production client configures HTTPS trust and timeouts");
  require(url.startsWith("https://api.svitlobot.in.ua/channelPing?channel_key=K&"),
          "recovered board sends the configured Svitlobot heartbeat URL");
  requests.push_back({nowMs, static_cast<uint32_t>(time(nullptr)), url});
  return 200;
}

namespace svb {
void Diagnostics::add(EventLevel, const String&) {}
}  // namespace svb

int main() {
  delayedRouter(30000, false);
  delayedRouter(180000, false);
  delayedRouter(15 * 60000, false);
  delayedRouter(24 * 60 * 60000, false);
  delayedRouter(180000, true);
  delayedRouter(180000, false, true);
  std::printf("Wi-Fi to reporting integration: %u checks passed\n", checks);
}
