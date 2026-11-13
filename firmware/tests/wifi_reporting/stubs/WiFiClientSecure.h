#pragma once
#include <WiFi.h>

class WiFiClientSecure : public WiFiClient {
 public:
  WiFiClientSecure() { secure = true; }
  void setCACert(const char* certificate) { trusted = certificate && certificate[0]; }
  void setHandshakeTimeout(uint32_t seconds) { handshakeSet = seconds > 0; }
  void setTimeout(uint32_t seconds) { timeoutSet = handshakeSet && seconds > 0; }
 private:
  bool handshakeSet = false;
};
