#include <Arduino.h>

#include "AppConfig.h"
#include "BootRecovery.h"
#include "ConfigStore.h"
#include "Diagnostics.h"
#include "PowerMonitor.h"
#include "SvitlobotClient.h"
#include "WebApp.h"
#include "WifiManager.h"

namespace {

svb::AppConfig config;
svb::Diagnostics diagnostics;
svb::ConfigStore configStore(diagnostics);
svb::BootRecovery bootRecovery(configStore, diagnostics);
svb::WifiManager wifi(config, diagnostics);
svb::SvitlobotClient svitlobot(config, wifi, diagnostics);
svb::PowerMonitor power(config, diagnostics);
svb::WebApp web(config, configStore, wifi, svitlobot, power, diagnostics);

}  // namespace

void setup() {
#ifdef SVB_RELAY_GPIO
  // A Sonoff build is an indicator, not a remote switch. Force the active-high
  // relay off before Serial, networking, or settings work begins.
  digitalWrite(SVB_RELAY_GPIO, LOW);
  pinMode(SVB_RELAY_GPIO, OUTPUT);
  digitalWrite(SVB_RELAY_GPIO, LOW);
#endif

  Serial.begin(115200);
  Serial.println();
  Serial.println(F("SvitloBoard 2 boot"));

  configStore.begin();
  configStore.load(config);
  if (bootRecovery.begin(config.setupComplete)) return;
  wifi.begin();
  power.begin();
  svitlobot.begin();
  web.begin();
}

void loop() {
  bootRecovery.loop();
  wifi.loop();
  power.loop();

  const svb::PowerMonitor::Event powerEvent = power.consumeEvent();
  if (powerEvent == svb::PowerMonitor::Event::MainsLost) {
    svitlobot.queueOffNotification();
  } else if (powerEvent == svb::PowerMonitor::Event::MainsRestored) {
    svitlobot.cancelQueuedOffNotification();
    svitlobot.requestPingNow();
  }

  uint8_t stableProfile = 0;
  if (wifi.consumeNewStableProfile(stableProfile) && config.lastWifiProfile != stableProfile) {
    svb::AppConfig candidate = config;
    candidate.lastWifiProfile = stableProfile;
    if (configStore.save(candidate)) config = candidate;
  }

  svitlobot.loop();
  web.loop();
  yield();
}
