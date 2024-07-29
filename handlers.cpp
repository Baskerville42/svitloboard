#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include "config.h"
#include "handlers.h"
#include "web_pages.h"
#include "connect_to_network.h"
#include "reset_manager.h"

// Зовнішнє оголошення HTTP сервера
extern ESP8266WebServer server;

// Функція для обробки повідомлень
void handleMessage(String message) {
  String page = String(messagePage);
  page.replace("%MESSAGE_TEXT%", message);
  page.replace("%HEAD_PARTITION%", headPartition);
  page.replace("%FOOTER_PARTITION%", footerPartition);
  server.send(200, "text/html", page);  // Надсилання сторінки з повідомленням
}

// Обробник кореневої сторінки
void handleRoot() {
  if (!config.setup_completed) {
    server.sendHeader("Location", "/setup", true);
  } else {
    server.sendHeader("Location", "/channel_key", true);
  }
  server.send(302, "text/plain", "");  // Перенаправлення на відповідну сторінку
}

// Обробник сторінки налаштувань
void handleSetup() {
  if (config.setup_completed) {
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
    return;
  }

  if (server.method() == HTTP_POST) {
    if (server.hasArg("ssid") && server.hasArg("password") && server.hasArg("channel_key")) {
      strcpy(config.ssid, server.arg("ssid").c_str());
      strcpy(config.password, server.arg("password").c_str());
      strcpy(config.channel_key, server.arg("channel_key").c_str());
      config.setup_completed = true;
      saveConfig();
      WiFi.begin(config.ssid, config.password);
      handleMessage(String("Пристрій налаштовано та буде перезавантажено. SvitloBoard буде підключено до мережі : ") + config.ssid + String("."));
      delay(2000);
      ESP.restart();  // Перезавантаження пристрою
    } else {
      handleMessage("Missing SSID, password or channel key.");
    }
  } else {
    String page = String(setupPage);
    page.replace("%HEAD_PARTITION%", headPartition);
    page.replace("%FOOTER_PARTITION%", footerPartition);
    server.send(200, "text/html", page);  // Відображення сторінки налаштувань
  }
}

// Обробник сторінки налаштування ключа каналу
void handleChannelKey() {
  if (!config.setup_completed) {
    server.sendHeader("Location", "/setup", true);
    server.send(302, "text/plain", "");
    return;
  }

  if (server.method() == HTTP_POST) {
    if (server.hasArg("channel_key")) {
      strcpy(config.channel_key, server.arg("channel_key").c_str());
      saveConfig();
      server.sendHeader("Location", "/channel_key", true);
      server.send(302, "text/plain", "");
    } else {
      handleMessage("Missing channel key.");
    }
  } else {
    String page = String(channelKeyPage);
    page.replace("%CHANNEL_KEY%", config.channel_key);
    page.replace("%HEAD_PARTITION%", headPartition);
    page.replace("%FOOTER_PARTITION%", footerPartition);
    server.send(200, "text/html", page);  // Відображення сторінки налаштування ключа каналу
  }
}

// Обробник сторінки налаштування Wi-Fi
void handleWiFi() {
  if (!config.setup_completed) {
    server.sendHeader("Location", "/setup", true);
    server.send(302, "text/plain", "");
    return;
  }

  if (server.method() == HTTP_POST) {
    if (server.hasArg("ssid") && server.hasArg("password")) {
      strcpy(config.ssid, server.arg("ssid").c_str());
      strcpy(config.password, server.arg("password").c_str());
      saveConfig();
      WiFi.disconnect();
      delay(100);
      connectToNetwork();
      server.sendHeader("Location", "/wifi", true);
      server.send(302, "text/plain", "");
    } else if (server.hasArg("backup1_ssid") && server.hasArg("backup1_password")) {
      strcpy(config.backup1_ssid, server.arg("backup1_ssid").c_str());
      strcpy(config.backup1_password, server.arg("backup1_password").c_str());
      saveConfig();
    } else if (server.hasArg("backup2_ssid") && server.hasArg("backup2_password")) {
      strcpy(config.backup2_ssid, server.arg("backup2_ssid").c_str());
      strcpy(config.backup2_password, server.arg("backup2_password").c_str());
      saveConfig();
    } else {
      handleMessage("Missing SSID or password.");
    }
  } else {
    String page = String(wifiPage);
    page.replace("%SSID%", config.ssid);
    page.replace("%PASSWORD%", config.password);
    page.replace("%BACKUP_1_SSID%", config.backup1_ssid);
    page.replace("%BACKUP_1_PASSWORD%", config.backup1_password);
    page.replace("%BACKUP_2_SSID%", config.backup2_ssid);
    page.replace("%BACKUP_2_PASSWORD%", config.backup2_password);
    page.replace("%HEAD_PARTITION%", headPartition);
    page.replace("%FOOTER_PARTITION%", footerPartition);
    server.send(200, "text/html", page);  // Відображення сторінки налаштування Wi-Fi
  }
}

// Обробник сторінки статусу
void handleStatus() {
  if (!config.setup_completed) {
    server.sendHeader("Location", "/setup", true);
    server.send(302, "text/plain", "");
    return;
  }

  String page = String(statusPage);
  page.replace("%CHANNEL_KEY%", config.channel_key);
  page.replace("%SSID%", config.ssid);
  page.replace("%IP_ADDRESS%", WiFi.localIP().toString());
  page.replace("%HEAD_PARTITION%", headPartition);
  page.replace("%FOOTER_PARTITION%", footerPartition);
  server.send(200, "text/html", page);  // Відображення сторінки статусу
}

// Обробник сторінки скидання до заводських налаштувань
void handleResetDevice() {
  if (server.method() == HTTP_POST) {
    handleMessage(String("Пристрій було повернуто до заводських налаштувань. Для повторного налаштування підключіться до мережі SVITLOBOARD та перейдіть на сторінку http://192.168.4.1/."));

    resetToFactorySettings();  // Скидання до заводських налаштувань
  }
}

// Обробник для невідомих сторінок
void handleNotFound() {
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");  // Перенаправлення на кореневу сторінку
}
