#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include "Ticker.h"
#include "config.h"
#include "setup_http_server.h"
#include "connect_to_network.h"
#include "create_soft_ap.h"
#include "channel_ping_request.h"
#include "reset_manager.h"

// Ініціалізація HTTP сервера на порту 80
ESP8266WebServer server(80);

const char* hostname = "SvitloBoard 1.0";  // Ім'я хоста для пристрою

// Налаштування таймерів
Ticker checkConnectionTimer(checkConnection, 30000); // Перевіряти стан кожні 5 секунд
Ticker channelPingTimer(channelPingRequest, 60000); // Виконання запиту channelPingRequest кожну хвилину

void setup() {
  Serial.begin(115200);  // Налаштування серійного зв'язку для відлагодження

  EEPROM.begin(512);  // Ініціалізація EEPROM з розміром 512 байт

  loadConfig();  // Завантаження конфігурації з EEPROM

  initializeResetManager();  // Ініціалізація системи управління скиданням

  if (!config.setup_completed) {
    createSoftAP();  // Створення точки доступу, якщо налаштування не завершені
  } else {
    // Запуск таймерів
    checkConnectionTimer.start();  // Підключення до мережі, якщо налаштування завершені
    channelPingTimer.start();    
  }

  setupHTTPServer();  // Налаштування HTTP сервера
}

void loop() {
  server.handleClient();  // Обробка запитів клієнтів

  // Перевірка таймерів
  checkConnectionTimer.update();  
  channelPingTimer.update();
}
