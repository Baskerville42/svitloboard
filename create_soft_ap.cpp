#include <ESP8266WiFi.h>
#include "config.h"

// Зовнішнє оголошення конфігурації
extern Config config;

// Функція для створення точки доступу на ESP8266
void createSoftAP() {
  // Створюємо точку доступу з SSID "SVITLOBOARD" та без пароля
  WiFi.softAP(config.ssid, config.password);

  // Виводимо IP-адресу точки доступу в серійну консоль
  Serial.print(String("SvitloBoard IP address: ") + WiFi.softAPIP().toString());
}
