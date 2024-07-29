#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include "config.h"

// Зовнішні оголошення змінних
extern const char* hostname;
extern Config config;

// Константи для API
const char* apiHost = "api.svitlobot.in.ua";
const int httpsPort = 443;  // Порт для HTTPS
const char* apiURL = "/channelPing?channel_key=";
const char* fingerprint = "8a:12:84:fe:41:6d:ee:9b:db:ff:29:15:a3:40:57:8b:68:2f:89:c5";  // Відбиток сертифікату сервера СвітлоБот. В разі потреби, новий відбиток можна перевірити тут https://iplocation.io/ssl-certificate-fingerprint

// Функція для перевірки доступності каналу через HTTPS запит
void channelPingRequest() {
  // Перевірка статусу WiFi та налаштувань
  if (WiFi.status() == WL_CONNECTED && config.setup_completed) {
    WiFiClientSecure client;
    client.setFingerprint(fingerprint);  // Встановлюємо сертифікат палець для безпеки

    Serial.print("Connecting to ");
    Serial.println(apiHost);

    // Спроба з'єднання з сервером
    if (!client.connect(apiHost, httpsPort)) {
      Serial.println("Connection failed");
      return;
    }

    // Формування запиту
    String request = String("GET ") + apiURL + config.channel_key + " HTTP/1.1\r\n" + "Host: " + apiHost + "\r\n" + "User-Agent: " + hostname + "\r\n" + "Connection: close\r\n\r\n";

    client.print(request);

    Serial.println("Request sent");

    // Очікування відповіді
    unsigned long startTime = millis();
    while (client.connected() && !client.available()) {
      if (millis() - startTime > 5000) {  // Тайм-аут 5 секунд
        Serial.println("Response timeout");
        client.stop();
        return;
      }
      delay(1);
    }

    // Перевірка статусу HTTP
    String responseLine;
    bool statusFound = false;

    while (client.connected() || client.available()) {
      responseLine = client.readStringUntil('\n');
      if (responseLine.startsWith("HTTP/1.1 200")) {
        statusFound = true;
        break;
      }
    }

    if (statusFound) {
      Serial.println("HTTP 200 OK");
    } else {
      Serial.println("HTTP status not 200 OK");
    }

    client.stop();  // Закриття з'єднання
  } else {
    Serial.println("WiFi not connected or channel_key not set");
  }
}
