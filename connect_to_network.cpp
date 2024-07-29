#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include "config.h"

// Зовнішнє оголошення змінної hostname
extern const char* hostname;
// Зовнішнє оголошення конфігурації
extern Config config;

const char* host = "http://api.svitlobot.in.ua";  // Хост для перевірки доступу до інтернету

// Функція для перевірки доступу до хоста
bool checkHostAccess(const char* host) {
  Serial.println("Checking if test host is reachable...");
  Serial.print(host);

  WiFiClient wifiClient;
  HTTPClient http;

  http.begin(wifiClient, host);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    Serial.println(" is reachable");
    http.end();
    return true;
  }

  Serial.println(" is not reachable");
  http.end();
  return false;
}

// Функція для підключення пристрою до Wi-Fi мережі
void connectToNetwork() {
  const char* ssids[] = {config.ssid, config.backup1_ssid, config.backup2_ssid};
  const char* passwords[] = {config.password, config.backup1_password, config.backup2_password};
  const int numNetworks = sizeof(ssids) / sizeof(ssids[0]);
  bool connected = false;

  WiFi.hostname(hostname);  // Встановлюємо ім'я хоста

  for (int i = 0; i < numNetworks; i++) {
    if (ssids[i] == nullptr || passwords[i] == nullptr || strlen(ssids[i]) == 0) {
      continue; // Пропускаємо порожні SSID
    }

    Serial.print("Trying to connect to: ");
    Serial.println(ssids[i]);

    WiFi.begin(ssids[i], passwords[i]);  // Починаємо підключення до Wi-Fi

    unsigned long startAttemptTime = millis();
    const unsigned long timeout = 20000;  // Тайм-аут 20 секунд

    // Очікуємо підключення до Wi-Fi
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < timeout) {
      delay(500);
      Serial.print(".");
    }

    // Перевірка результату підключення
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("");
      Serial.println("WiFi connected");
      Serial.println(String("SSID: ") + WiFi.SSID());
      Serial.println(String("IP address: ") + WiFi.localIP().toString());

      // Перевірка доступу до хоста
      if (checkHostAccess(host)) {
        Serial.println("Host is accessible.");
        connected = true;
        break;  // Якщо підключення успішне і є доступ до хоста, виходимо з циклу
      } else {
        Serial.println("Host is not accessible. Trying next network...");
        WiFi.disconnect();  // Відключаємось від поточної мережі
      }
    } else {
      Serial.println("");
      Serial.println("WiFi connection failed.");
    }
  }

  // Якщо не вдалося підключитися до жодної мережі або інтернет недоступний
  if (!connected) {
    Serial.println("Failed to connect to any network.");
    // Спробувати залишитися на останній мережі, якщо тільки одна мережа збережена
    if (numNetworks == 1) {
      WiFi.begin(ssids[0], passwords[0]);
      while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
      }
      Serial.println("Connected to the only available network, waiting for internet access...");
      while (!checkHostAccess(host)) {
        delay(5000);  // Перевіряти доступ кожні 5 секунд
        Serial.println("Still waiting for internet access...");
      }
      Serial.println("Internet access restored.");
    }
  }
}

void checkConnection() {
  if (!checkHostAccess(host)) {
    Serial.println("Connection lost. Reconnecting...");
    connectToNetwork();
  }
}
