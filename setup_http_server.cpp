#include <ESP8266WebServer.h>
#include "handlers.h"

// Зовнішнє оголошення HTTP сервера
extern ESP8266WebServer server;

// Функція для налаштування HTTP сервера та маршрутизації запитів
void setupHTTPServer() {
  server.on("/", handleRoot);                      // Обробка запитів до кореневої сторінки
  server.on("/setup", handleSetup);                // Обробка запитів до сторінки налаштувань
  server.on("/channel_key", handleChannelKey);     // Обробка запитів до сторінки налаштування ключа каналу
  server.on("/wifi", handleWiFi);                  // Обробка запитів до сторінки налаштування Wi-Fi
  server.on("/status", handleStatus);              // Обробка запитів до сторінки статусу
  server.on("/factory-reset", handleResetDevice);  // Обробка запитів до сторінки скидання до заводських налаштувань
  server.onNotFound(handleNotFound);               // Обробка запитів до невідомих сторінок

  server.begin();                         // Запуск HTTP сервера
  Serial.println("HTTP server started");  // Повідомлення про запуск HTTP сервера
}
