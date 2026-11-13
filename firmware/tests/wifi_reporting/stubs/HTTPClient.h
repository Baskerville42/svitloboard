#pragma once
#include <WiFi.h>

int hostGet(const String& url, bool secure, bool trusted, bool timeoutSet);

class HTTPClient {
 public:
  void setConnectTimeout(uint32_t) {}
  void setTimeout(uint32_t) {}
  void useHTTP10(bool) {}
  bool begin(WiFiClient& client, const String& url) {
    client_ = &client;
    url_ = url;
    return true;
  }
  void addHeader(const String&, const String&) {}
  int GET() { return hostGet(url_, client_->secure, client_->trusted, client_->timeoutSet); }
  void end() {}
 private:
  WiFiClient* client_ = nullptr;
  String url_;
};
