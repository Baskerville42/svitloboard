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
  // Обнуляємо структуру для коректної міграції зі старих версій конфігурації
  memset(&config, 0, sizeof(config));
  EEPROM.get(0, config);
  delay(1000);

  // Гарантуємо нуль-термінацію нових полів навіть при застарілому EEPROM
  config.ssid[sizeof(config.ssid) - 1] = '\0';
  config.password[sizeof(config.password) - 1] = '\0';
  config.backup1_ssid[sizeof(config.backup1_ssid) - 1] = '\0';
  config.backup1_password[sizeof(config.backup1_password) - 1] = '\0';
  config.backup2_ssid[sizeof(config.backup2_ssid) - 1] = '\0';
  config.backup2_password[sizeof(config.backup2_password) - 1] = '\0';
  config.channel_key[sizeof(config.channel_key) - 1] = '\0';
  config.fingerprint[sizeof(config.fingerprint) - 1] = '\0';

  // Санітизація нового поля force_http для старих EEPROM: значення тільки 0 або 1
  if (config.force_http != 0 && config.force_http != 1) {
    config.force_http = false;
  }

  if (!config.setup_completed) {
    strcpy(config.ssid, defaultSSID);
    strcpy(config.password, defaultPassword);
    config.fingerprint[0] = '\0'; // За замовчуванням fingerprint не заданий
    config.force_http = false;     // За замовчуванням використовуємо HTTPS
  }

  Serial.println("Current config:");
  Serial.println(String("Main SSID: ") + config.ssid);
  Serial.println(String("Password: ") + config.password);
  Serial.println(String("Backup 1 SSID: ") + config.backup1_ssid);
  Serial.println(String("Password: ") + config.backup1_password);
  Serial.println(String("Backup 2 SSID: ") + config.backup2_ssid);
  Serial.println(String("Password: ") + config.backup2_password);
  Serial.println(String("Channel Key: ") + config.channel_key);
  Serial.println(String("SSL Fingerprint: ") + (config.fingerprint[0] ? String(config.fingerprint) : String("<not set>")));
  Serial.println(String("Force HTTP: ") + (config.force_http ? "true" : "false"));
  Serial.println(String("Setup completed: ") + (config.setup_completed ? "true" : "false"));
}
