#ifndef HANDLERS_H
#define HANDLERS_H

// Функція для обробки запитів до кореневої сторінки
void handleRoot();

// Функція для обробки запитів до сторінки налаштувань
void handleSetup();

// Функція для обробки запитів до сторінки налаштування ключа каналу
void handleChannelKey();

// Функція для обробки запитів до сторінки налаштування Wi-Fi
void handleWiFi();

// Функція для обробки запитів до сторінки статусу
void handleStatus();

// Функція для обробки запитів до сторінки скидання до заводських налаштувань
void handleResetDevice();

// Функція для обробки запитів до невідомих сторінок
void handleNotFound();

#endif  // HANDLERS_H
