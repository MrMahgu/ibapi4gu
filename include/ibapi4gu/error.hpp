#pragma once

#include <string>

namespace ibapi4gu {

enum class ErrorCode {
  none,
  invalid_operation,
  connection_failed,
  connection_timeout,
  disconnected,
  request_failed,
  timeout,
  cancelled,
  callback_exception,
  shutdown
};

struct Error {
  ErrorCode code;
  std::string message;
  int ibkr_code;
  long long timestamp_ms;
  int related_id;

  Error()
      : code(ErrorCode::none), ibkr_code(0), timestamp_ms(0), related_id(-1) {}

  Error(ErrorCode error_code, const std::string& error_message,
        int vendor_code = 0, long long vendor_timestamp_ms = 0,
        int id = -1)
      : code(error_code),
        message(error_message),
        ibkr_code(vendor_code),
        timestamp_ms(vendor_timestamp_ms),
        related_id(id) {}
};

}  // namespace ibapi4gu
