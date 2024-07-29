#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include "config.h"

const uint8_t RESET_THRESHOLD = 4;  // Поріг для скидання до заводських налаштувань

// Функція для відновлення заводських налаштувань
void resetToFactorySettings() {
  // Очищення EEPROM
  for (int i = 0; i < EEPROM.length(); i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();  // Підтвердження змін
  delay(2000);      // Затримка для стабілізації
  ESP.restart();    // Перезавантаження пристрою
}

// Функція для скидання лічильника запусків на 0
void resetBootCount() {
  config.bootCount = 0;   // Скидання лічильника
  EEPROM.put(0, config);  // Запис нових налаштувань у EEPROM
  EEPROM.commit();        // Підтвердження змін
}

// Функція для ініціалізації менеджера скидання
void initializeResetManager() {
  config.bootCount++;     // Збільшення лічильника запусків
  EEPROM.put(0, config);  // Запис нових налаштувань у EEPROM
  EEPROM.commit();        // Підтвердження змін

  Serial.print("Boot count: ");
  Serial.println(config.bootCount);

  // Перевірка на досягнення порогу
  if (config.bootCount == RESET_THRESHOLD) {
    resetToFactorySettings();  // Скидання до заводських налаштувань
  } else {
    delay(5000);       // Затримка перед наступним скиданням лічильника
    resetBootCount();  // Скидання лічильника запусків
  }
}
