#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include "config.h"

// Зовнішні оголошення змінних
extern const char* hostname;
extern Config config;

// Константи для API
const char* apiHost = "api.svitlobot.in.ua";
const int httpsPort = 443;  // Порт для HTTPS
const int httpPort = 80;    // Порт для HTTP
const char* apiURL = "/channelPing?channel_key=";
// Примітка: За замовчуванням пінінг сертифікату вимкнено (див. нижче). Користувач може вказати fingerprint у веб-інтерфейсі.

// Функція для перевірки доступності каналу через HTTP/HTTPS запит
void channelPingRequest() {
  // Перевірка статусу WiFi та налаштувань
  if (WiFi.status() == WL_CONNECTED && config.setup_completed) {
    int port = httpsPort;
    Client* clientPtr = nullptr;
    WiFiClient httpClient;
    WiFiClientSecure httpsClient;

    if (config.force_http) {
      // HTTP режим (небезпечно)
      clientPtr = &httpClient;
      port = httpPort;
      Serial.println("Mode: HTTP (insecure)");
    } else {
      // HTTPS режим
      if (config.fingerprint[0] != '\0') {
        httpsClient.setFingerprint(config.fingerprint);  // Пінінг, якщо вказано fingerprint
        Serial.print("Mode: HTTPS with fingerprint pinning ");
        Serial.println(config.fingerprint);
      } else {
        httpsClient.setInsecure();  // Без пінінгу: довіряємо з'єднанню (на свій ризик)
        Serial.println("Mode: HTTPS without pinning (insecure)");
      }
      clientPtr = &httpsClient;
      port = httpsPort;
    }

    Serial.print("Connecting to ");
    Serial.print(apiHost);
    Serial.print(":");
    Serial.println(port);

    // Спроба з'єднання з сервером
    if (!clientPtr->connect(apiHost, port)) {
      Serial.println("Connection failed");
      clientPtr->stop();
      return;
    }

    // Формування запиту
    String request = String("GET ") + apiURL + config.channel_key + " HTTP/1.1\r\n" +
                     "Host: " + apiHost + "\r\n" +
                     "User-Agent: " + hostname + "\r\n" +
                     "Connection: close\r\n\r\n";

    clientPtr->print(request);

    Serial.println("Request sent");

    // Очікування відповіді
    unsigned long startTime = millis();
    while (clientPtr->connected() && !clientPtr->available()) {
      if (millis() - startTime > 5000) {  // Тайм-аут 5 секунд
        Serial.println("Response timeout");
        clientPtr->stop();
        return;
      }
      delay(1);
    }

    // Перевірка статусу HTTP
    String responseLine;
    bool statusFound = false;

    while (clientPtr->connected() || clientPtr->available()) {
      responseLine = clientPtr->readStringUntil('\n');
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

    clientPtr->stop();  // Закриття з'єднання
  } else {
    Serial.println("WiFi not connected or channel_key not set");
  }
}
