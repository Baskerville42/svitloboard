#ifndef CONFIG_H
#define CONFIG_H

// Структура для зберігання конфігураційних даних
struct Config {
  char ssid[32];              // SSID основної мережі
  char password[32];          // Пароль основної мережі
  char backup1_ssid[32];      // SSID резервної мережі 1
  char backup1_password[32];  // Пароль резервної мережі 1
  char backup2_ssid[32];      // SSID резервної мережі 2
  char backup2_password[32];  // Пароль резервної мережі 2
  char channel_key[32];       // Ключ каналу
  bool setup_completed;       // Флаг завершення налаштування
  uint8_t bootCount;          // Лічильник завантажень для управління скиданням до заводських налаштувань
};

extern Config config;  // Оголошення зовнішньої змінної config

void saveConfig();  // Функція для збереження конфігурації в EEPROM
void loadConfig();  // Функція для завантаження конфігурації з EEPROM

#endif  // CONFIG_H
