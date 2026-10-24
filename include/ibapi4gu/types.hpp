#pragma once

#include <chrono>
#include <string>

namespace ibapi4gu {

class RequestId {
 public:
  explicit RequestId(int value = -1) : value_(value) {}
  int value() const { return value_; }
  bool operator==(RequestId other) const { return value_ == other.value_; }
  bool operator!=(RequestId other) const { return !(*this == other); }
  bool operator<(RequestId other) const { return value_ < other.value_; }

 private:
  int value_;
};

class OrderId {
 public:
  explicit OrderId(int value = -1) : value_(value) {}
  int value() const { return value_; }
  bool operator==(OrderId other) const { return value_ == other.value_; }
  bool operator!=(OrderId other) const { return !(*this == other); }
  bool operator<(OrderId other) const { return value_ < other.value_; }

 private:
  int value_;
};

enum class SessionState { connecting, ready, disconnecting, disconnected };

struct SessionOptions {
  std::string host;
  int port;
  int client_id;
  bool extra_auth;
  std::chrono::milliseconds connect_timeout;

  SessionOptions()
      : host("127.0.0.1"),
        port(7497),
        client_id(0),
        extra_auth(false),
        connect_timeout(std::chrono::seconds(30)) {}
};

struct RequestOptions {
  std::chrono::milliseconds timeout;

  RequestOptions() : timeout(std::chrono::seconds(30)) {}
  explicit RequestOptions(std::chrono::milliseconds value) : timeout(value) {}
};

}  // namespace ibapi4gu
