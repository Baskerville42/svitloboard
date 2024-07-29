#include <Arduino.h>
#include <EEPROM.h>
#include "config.h"

Config config;

const char* defaultSSID = "SVITLOBOARD";
const char* defaultPassword = "";

void saveConfig() {
  EEPROM.put(0, config);
  EEPROM.commit();
}

void loadConfig() {
  EEPROM.get(0, config);
  delay(1000);
  if (!config.setup_completed) {
    strcpy(config.ssid, defaultSSID);
    strcpy(config.password, defaultPassword);
  }

  Serial.println("Current config:");
  Serial.println(String("Main SSID: ") + config.ssid);
  Serial.println(String("Password: ") + config.password);
  Serial.println(String("Backup 1 SSID: ") + config.backup1_ssid);
  Serial.println(String("Password: ") + config.backup1_password);
  Serial.println(String("Backup 2 SSID: ") + config.backup2_ssid);
  Serial.println(String("Password: ") + config.backup2_password);
  Serial.println(String("Channel Key: ") + config.channel_key);
  Serial.println(String("Setup completed: ") + (config.setup_completed ? "true" : "false"));
}
