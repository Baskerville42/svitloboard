#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <WiFi.h>
#include "WifiManager.h"

namespace {

uint32_t nowMs;
unsigned checks;

void require(bool condition, const char* message) {
  ++checks;
  if (!condition) {
    std::fprintf(stderr, "FAIL at %u ms: %s\n", nowMs, message);
    std::exit(1);
  }
}

svb::AppConfig configured() {
  svb::AppConfig config{};
  config.setupComplete = 1;
  std::strcpy(config.deviceName, "SvitloBoard");
  config.wifi[0].enabled = 1;
  std::strcpy(config.wifi[0].ssid, "home");
  std::strcpy(config.wifi[0].password, "password");
  return config;
}

void reset(uint32_t start = 100) {
  nowMs = start;
  WiFi = HostWiFi{};
}

void runFor(svb::WifiManager& manager, uint32_t duration) {
  // Subtraction intentionally handles millis() overflow, just like the device.
  const uint32_t start = nowMs;
  while (nowMs - start < duration) {
    const uint32_t remaining = duration - (nowMs - start);
    nowMs += remaining < 100 ? remaining : 100;
    manager.loop();
  }
}

void waitForConnection(svb::WifiManager& manager) {
  // Covers the maximum retry delay, a complete profile round, and loop jitter.
  const uint32_t start = nowMs;
  const uint32_t recoveryBound = svb::kWifiMaximumBackoffMs +
      svb::kWifiProfileCount * svb::kWifiAttemptMs + 20000;
  while (!manager.connected() && nowMs - start < recoveryBound) {
    runFor(manager, 100);
  }
  manager.loop();
  require(manager.connected(), "a router appearing later must reconnect without user input");
  require(manager.state() == svb::WifiManager::State::Connected, "connection state must follow DHCP success");
}

void testStaleFailureHasFullAttemptWindow(wl_status_t failure) {
  reset();
  auto config = configured();
  svb::Diagnostics diagnostics;
  svb::WifiManager manager(config, diagnostics);
  WiFi.currentStatus = failure;
  WiFi.networkAvailable = true;
  WiFi.associationDelayMs = 6000;
  manager.begin();
  runFor(manager, 5900);
  require(WiFi.attempts.size() == 1, "a stale failure must not abort a fresh scan/association");
  require(manager.state() == svb::WifiManager::State::Connecting, "fresh attempt remains active before DHCP");
  runFor(manager, 200);
  require(manager.connected(), "delayed DHCP succeeds despite a stale terminal status");
}

void testDelayedRouterAndRepeatedOutages() {
  reset();
  auto config = configured();
  const auto original = config;
  svb::Diagnostics diagnostics;
  svb::WifiManager manager(config, diagnostics);
  manager.begin();
  require(!WiFi.persistentEnabled && !WiFi.autoReconnectEnabled, "manager owns retries without SDK persistence");
  require(WiFi.currentMode == WIFI_STA, "configured startup begins as a station");
  runFor(manager, 15 * 60 * 1000);
  require(manager.apActive() && WiFi.accessPoint, "a long outage exposes recovery AP");
  require(WiFi.currentMode == WIFI_AP_STA, "recovery AP keeps station functionality enabled");
  require(WiFi.attempts.size() >= 5, "recovery AP continues attempts throughout a long outage");
  bool reachedRetryCap = false;
  const uint32_t maximumRetryGap = svb::kWifiMaximumBackoffMs + svb::kWifiAttemptMs;
  for (size_t index = 1; index < WiFi.attempts.size(); ++index) {
    const uint32_t gap = WiFi.attempts[index].startedAt - WiFi.attempts[index - 1].startedAt;
    require(gap <= maximumRetryGap + 100,
            "single-profile retries stay within capped backoff plus attempt time");
    reachedRetryCap = reachedRetryCap || gap >= maximumRetryGap;
  }
  require(reachedRetryCap, "long outage exercises the maximum retry backoff");
  require(nowMs - WiFi.attempts.back().startedAt <= maximumRetryGap + 100,
          "latest recovery attempt remains within capped retry gap");
  require(std::memcmp(&config, &original, sizeof(config)) == 0, "unavailable router must not erase credentials");
  WiFi.networkAvailable = true;
  waitForConnection(manager);
  require(manager.activeProfile() == 0, "late router uses saved profile");
  require(manager.connectedSsid() == "home", "saved SSID is connected");
  require(manager.stationIp() == "192.168.1.42", "station address is exposed after DHCP");
  require(manager.apActive(), "recovery AP remains during handoff grace");
  runFor(manager, svb::kProvisioningApGraceMs - 100);
  require(manager.apActive(), "handoff grace is not shortened");
  runFor(manager, 100);
  require(!manager.apActive() && !WiFi.accessPoint && WiFi.currentMode == WIFI_STA,
          "recovery AP stops after successful handoff");
  WiFi.networkAvailable = false;
  runFor(manager, 30000);
  require(!manager.connected() && manager.apActive(), "later outage returns to recovery");
  WiFi.networkAvailable = true;
  waitForConnection(manager);
  require(std::memcmp(&config, &original, sizeof(config)) == 0, "repeat recovery preserves config");
}

void testNoCredentials() {
  for (bool setupComplete : {false, true}) {
    reset();
    svb::AppConfig config{};
    config.setupComplete = setupComplete;
    svb::Diagnostics diagnostics;
    svb::WifiManager manager(config, diagnostics);
    manager.begin();
    runFor(manager, 20 * 60 * 1000);
    require(manager.state() == svb::WifiManager::State::NoCredentials && manager.apActive(),
            "unconfigured device stays available for provisioning");
    require(WiFi.attempts.empty(), "empty profiles do not trigger station attempts");
  }
}

void testProfileOrderAndStableSelection() {
  reset();
  auto config = configured();
  config.lastWifiProfile = 2;
  config.wifi[1].enabled = 1;
  std::strcpy(config.wifi[1].ssid, "backup");
  config.wifi[2].enabled = 1;
  std::strcpy(config.wifi[2].ssid, "preferred");
  svb::Diagnostics diagnostics;
  svb::WifiManager manager(config, diagnostics);
  manager.begin();
  require(WiFi.attempts.front().ssid == "preferred", "last successful profile is attempted first");
  // Full unavailable round leaves all three profiles represented in order.
  runFor(manager, 3 * svb::kWifiAttemptMs);
  require(WiFi.attempts.size() >= 3, "all saved profiles receive attempts");
  require(WiFi.attempts[1].ssid == "home" && WiFi.attempts[2].ssid == "backup",
          "remaining profiles follow configured order");
  WiFi.availableSsid = "backup";
  WiFi.networkAvailable = true;
  waitForConnection(manager);
  require(manager.activeProfile() == 1, "available backup is selected automatically");
  uint8_t profile = 99;
  require(!manager.consumeNewStableProfile(profile), "new profile is not persisted before stable interval");
  runFor(manager, svb::kWifiStableMs);
  require(manager.consumeNewStableProfile(profile) && profile == 1, "stable backup is reported for persistence");
  require(!manager.consumeNewStableProfile(profile), "stable profile notification is consumed once");
}

void testMillisWrap() {
  reset(UINT32_MAX - 6000);
  auto config = configured();
  svb::Diagnostics diagnostics;
  svb::WifiManager manager(config, diagnostics);
  manager.begin();
  runFor(manager, 30000);
  require(manager.apActive(), "attempt and backoff timers survive millis overflow");
  WiFi.networkAvailable = true;
  waitForConnection(manager);
  runFor(manager, svb::kProvisioningApGraceMs);
  require(!manager.apActive(), "AP handoff remains functional after millis overflow");
}

void testRouterAppearsNearAttemptDeadline() {
  reset();
  auto config = configured();
  svb::Diagnostics diagnostics;
  svb::WifiManager manager(config, diagnostics);
  manager.begin();
  runFor(manager, svb::kWifiAttemptMs - 200);
  WiFi.networkAvailable = true;
  // The driver's first scan already failed. A new begin must occur after AP opens.
  runFor(manager, 200);
  require(manager.apActive(), "router appearance at timeout does not suppress recovery AP");
  waitForConnection(manager);
  require(WiFi.attempts.size() == 2, "router appearing at a failed scan boundary gets another attempt");
}

void testOutageHasNoLifetimeRetryLimit() {
  reset();
  auto config = configured();
  svb::Diagnostics diagnostics;
  svb::WifiManager manager(config, diagnostics);
  manager.begin();
  runFor(manager, 48U * 60 * 60 * 1000);
  require(manager.apActive() && WiFi.currentMode == WIFI_AP_STA,
          "two-day router absence retains both recovery AP and station mode");
  require(manager.state() != svb::WifiManager::State::NoCredentials && WiFi.attempts.size() > 500,
          "temporary absence never exhausts a lifetime retry budget");
  while (manager.state() != svb::WifiManager::State::Backoff) runFor(manager, 100);
  const size_t attemptsBeforeReturn = WiFi.attempts.size();
  WiFi.networkAvailable = true;
  waitForConnection(manager);
  require(WiFi.attempts.size() > attemptsBeforeReturn,
          "saved network returning after two days is retried automatically");
}

#if !defined(ESP8266)
void testSlowDhcpWhileRecoveryApActive() {
  reset();
  auto config = configured();
  svb::Diagnostics diagnostics;
  svb::WifiManager manager(config, diagnostics);
  manager.begin();
  runFor(manager, svb::kWifiAttemptMs);
  require(manager.apActive(), "initial failed scan opens recovery AP before router returns");
  WiFi.networkAvailable = true;
  WiFi.associationDelayMs = 3000;
  WiFi.dhcpDelayMs = 17000;
  runFor(manager, svb::kWifiInitialBackoffMs);
  const unsigned disconnects = WiFi.disconnects;
  runFor(manager, svb::kWifiAttemptMs - 100);
  require(WiFi.associated && !manager.connected(), "router associates before its DHCP service is ready");
  runFor(manager, 100);
  require(WiFi.disconnects == disconnects && WiFi.attempts.size() == 2,
          "scan deadline must not tear down an associated station waiting for DHCP");
  require(manager.apActive() && WiFi.currentMode == WIFI_AP_STA,
          "DHCP grace preserves the recovery access point");
  runFor(manager, 8000);
  require(manager.connected() && manager.state() == svb::WifiManager::State::Connected,
          "late DHCP completes without another station attempt");
  require(WiFi.disconnects == disconnects, "DHCP completion did not require reconnecting");
  runFor(manager, svb::kProvisioningApGraceMs);
  require(!manager.apActive(), "successful slow DHCP still completes recovery AP handoff");
}

void testDhcpGraceIsBoundedAndResetsPerAttempt() {
  reset();
  auto config = configured();
  svb::Diagnostics diagnostics;
  svb::WifiManager manager(config, diagnostics);
  WiFi.networkAvailable = true;
  WiFi.associationDelayMs = 3000;
  WiFi.dhcpAvailable = false;
  manager.begin();
  runFor(manager, svb::kWifiAttemptMs + svb::kWifiDhcpGraceMs - 100);
  require(WiFi.associated && manager.state() == svb::WifiManager::State::Connecting,
          "one associated attempt retains its complete bounded DHCP grace");
  runFor(manager, 100);
  require(manager.apActive() && manager.state() == svb::WifiManager::State::Backoff,
          "a permanently missing DHCP response cannot stop fallback and retries");
  runFor(manager, svb::kWifiInitialBackoffMs + svb::kWifiAttemptMs);
  require(WiFi.attempts.size() == 2 && WiFi.associated &&
              manager.state() == svb::WifiManager::State::Connecting,
          "each new associated attempt receives its own DHCP grace");
  WiFi.dhcpAvailable = true;
  runFor(manager, 100);
  require(manager.connected(), "DHCP becoming available during a later attempt restores station access");
}

void testAssociationLostDuringDhcpGrace() {
  reset();
  auto config = configured();
  svb::Diagnostics diagnostics;
  svb::WifiManager manager(config, diagnostics);
  WiFi.networkAvailable = true;
  WiFi.associationDelayMs = 3000;
  WiFi.dhcpAvailable = false;
  manager.begin();
  runFor(manager, svb::kWifiAttemptMs);
  require(WiFi.associated, "station has associated before router disappears again");
  WiFi.networkAvailable = false;
  runFor(manager, svb::kWifiDhcpGraceMs);
  require(manager.apActive() && manager.state() != svb::WifiManager::State::NoCredentials,
          "losing association during DHCP grace retains automatic recovery");
  WiFi.networkAvailable = true;
  WiFi.dhcpAvailable = true;
  waitForConnection(manager);
}
#endif

}  // namespace

HostWiFi WiFi;

uint32_t millis() { return nowMs; }

namespace svb {
void Diagnostics::add(EventLevel, const String&) {}
String accessPointName() { return "SvitloBoard-test"; }
}  // namespace svb

int main() {
  testStaleFailureHasFullAttemptWindow(WL_NO_SSID_AVAIL);
  testStaleFailureHasFullAttemptWindow(WL_CONNECT_FAILED);
  testDelayedRouterAndRepeatedOutages();
  testNoCredentials();
  testProfileOrderAndStableSelection();
  testMillisWrap();
  testRouterAppearsNearAttemptDeadline();
  testOutageHasNoLifetimeRetryLimit();
#if !defined(ESP8266)
  testSlowDhcpWhileRecoveryApActive();
  testDhcpGraceIsBoundedAndResetsPerAttempt();
  testAssociationLostDuringDhcpGrace();
#endif
  std::printf("Wi-Fi manager: %u checks passed\n", checks);
}
