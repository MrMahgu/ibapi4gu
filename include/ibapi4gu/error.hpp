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
  shutdown,
  ibkr_notice,
  ibkr_warning,
  ibkr_error,
  backend_failure
};

enum class ErrorSeverity { information, warning, error, fatal };

enum class ErrorScope {
  general,
  request,
  order,
  connection,
  account,
  market_data_farm,
  historical_data_farm,
  security_definition_farm
};

struct Error {
  ErrorCode code;
  std::string message;
  int ibkr_code;
  long long timestamp_ms;
  int related_id;
  ErrorSeverity severity;
  ErrorScope scope;
  bool terminal;
  std::string advanced_reject_json;

  Error()
      : code(ErrorCode::none),
        ibkr_code(0),
        timestamp_ms(0),
        related_id(-1),
        severity(ErrorSeverity::information),
        scope(ErrorScope::general),
        terminal(false) {}

  Error(ErrorCode error_code, const std::string& error_message,
        int vendor_code = 0, long long vendor_timestamp_ms = 0,
        int id = -1)
      : code(error_code),
        message(error_message),
        ibkr_code(vendor_code),
        timestamp_ms(vendor_timestamp_ms),
        related_id(id),
        severity(ErrorSeverity::error),
        scope(ErrorScope::general),
        terminal(true) {}
};

}  // namespace ibapi4gu
