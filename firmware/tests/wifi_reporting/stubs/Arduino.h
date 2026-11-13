#pragma once

#include "../../config_store/stubs/Arduino.h"
#include <cstring>

#define PROGMEM

using std::min;
uint32_t millis();
void configTime(long, int, const char*, const char*, const char*);

struct HostEsp {
  uint64_t getEfuseMac() const { return 0x123456789abcULL; }
};
extern HostEsp ESP;
