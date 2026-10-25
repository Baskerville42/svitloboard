#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <type_traits>

#define F(value) value
#define HIGH 1
#define HEX 16

class String {
 public:
  String() = default;
  String(const char* value) : value_(value) {}
  String(const std::string& value) : value_(value) {}
  template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
  String(T value, unsigned base = 10) {
    char buffer[32];
    if (base == HEX) snprintf(buffer, sizeof(buffer), "%llx", static_cast<unsigned long long>(value));
    else snprintf(buffer, sizeof(buffer), "%lld", static_cast<long long>(value));
    value_ = buffer;
  }
  size_t length() const { return value_.size(); }
  bool isEmpty() const { return value_.empty(); }
  const char* c_str() const { return value_.c_str(); }
  char operator[](size_t index) const { return value_[index]; }
  bool startsWith(const char* prefix) const { return value_.find(prefix) == 0; }
  int indexOf(char needle, size_t from = 0) const {
    const auto found = value_.find(needle, from);
    return found == std::string::npos ? -1 : static_cast<int>(found);
  }
  String substring(size_t start) const { return value_.substr(start); }
  void toUpperCase() {
    std::transform(value_.begin(), value_.end(), value_.begin(), [](unsigned char c) { return std::toupper(c); });
  }
  friend bool operator==(const String& left, const char* right) { return left.value_ == right; }
  friend String operator+(const String& left, const String& right) { return left.value_ + right.value_; }
 private:
  std::string value_;
};
