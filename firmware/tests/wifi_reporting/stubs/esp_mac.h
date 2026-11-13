#pragma once
#include <Arduino.h>

constexpr int ESP_OK = 0;
inline int esp_efuse_mac_get_default(uint8_t* mac) {
  const uint8_t fixed[6] = {2, 3, 4, 5, 6, 7};
  memcpy(mac, fixed, sizeof(fixed));
  return ESP_OK;
}
