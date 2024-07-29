#ifndef CONNECT_TO_NETWORK_H
#define CONNECT_TO_NETWORK_H

void connectToNetwork();                // Функція для підключення пристрою до Wi-Fi мережі
bool checkHostAccess(const char* host); // Функція перевірки наявності доступу до мережі інтернет
void checkConnection();                 // Функція перевірки параметрів підключення до мережі

#endif  // CONNECT_TO_NETWORK_H
