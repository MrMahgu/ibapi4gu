#include "detail/error_classifier.hpp"

namespace ibapi4gu {
namespace detail {
namespace {

bool is_fatal_connection_code(int id, int code) {
  switch (code) {
    case 326:  // Client ID is already in use.
    case 502:  // Could not connect to TWS.
    case 504:  // Not connected.
    case 505:  // Unknown inbound message ID.
    case 506:  // Unsupported protocol version.
    case 507:  // Socket/EOF failure.
    case 508:  // Malformed inbound message.
    case 509:  // Socket reader exception.
    case 520:  // Socket creation failed.
    case 1300:  // Socket port reset; connection will be dropped.
      return true;
    case 503:  // TWS is too old, or a requested function is unsupported.
      return id < 0;
    default:
      return false;
  }
}

bool is_order_code(int code) {
  switch (code) {
    case 103:
    case 104:
    case 105:
    case 106:
    case 107:
    case 109:
    case 110:
    case 111:
    case 113:
    case 114:
    case 115:
    case 116:
    case 117:
    case 118:
    case 119:
    case 120:
    case 121:
    case 122:
    case 123:
    case 124:
    case 125:
    case 126:
    case 127:
    case 128:
    case 129:
    case 130:
    case 131:
    case 132:
    case 133:
    case 134:
    case 135:
    case 136:
    case 137:
    case 138:
    case 139:
    case 140:
    case 141:
    case 145:
    case 146:
    case 147:
    case 148:
    case 151:
    case 152:
    case 153:
    case 154:
    case 155:
    case 156:
    case 157:
    case 158:
    case 159:
    case 201:
    case 202:
    case 203:
    case 2102:
    case 2109:
    case 2137:
    case 2168:
    case 2169:
      return true;
    default:
      return false;
  }
}

Error make_error(ErrorCode error_code, ErrorSeverity severity,
                 ErrorScope scope, bool terminal, int id,
                 long long timestamp_ms, int code,
                 const std::string& message,
                 const std::string& advanced_reject_json) {
  Error error(error_code, message, code, timestamp_ms, id);
  error.severity = severity;
  error.scope = scope;
  error.terminal = terminal;
  error.advanced_reject_json = advanced_reject_json;
  return error;
}

}  // namespace

Error classify_ibkr_error(int id, long long timestamp_ms, int code,
                          const std::string& message,
                          const std::string& advanced_reject_json) {
  if (is_fatal_connection_code(id, code)) {
    return make_error(ErrorCode::connection_failed, ErrorSeverity::fatal,
                      ErrorScope::connection, true, id, timestamp_ms, code,
                      message, advanced_reject_json);
  }

  switch (code) {
    case 1100:
    case 2110:
      return make_error(ErrorCode::ibkr_warning, ErrorSeverity::warning,
                        ErrorScope::connection, false, id, timestamp_ms, code,
                        message, advanced_reject_json);
    case 1101:
      return make_error(ErrorCode::ibkr_warning, ErrorSeverity::warning,
                        ErrorScope::connection, false, id, timestamp_ms, code,
                        message, advanced_reject_json);
    case 1102:
      return make_error(ErrorCode::ibkr_notice, ErrorSeverity::information,
                        ErrorScope::connection, false, id, timestamp_ms, code,
                        message, advanced_reject_json);
    case 2103:
      return make_error(ErrorCode::ibkr_warning, ErrorSeverity::warning,
                        ErrorScope::market_data_farm, false, id, timestamp_ms,
                        code, message, advanced_reject_json);
    case 2104:
    case 2108:
      return make_error(ErrorCode::ibkr_notice, ErrorSeverity::information,
                        ErrorScope::market_data_farm, false, id, timestamp_ms,
                        code, message, advanced_reject_json);
    case 2105:
      return make_error(ErrorCode::ibkr_warning, ErrorSeverity::warning,
                        ErrorScope::historical_data_farm, false, id,
                        timestamp_ms, code, message, advanced_reject_json);
    case 2106:
    case 2107:
      return make_error(ErrorCode::ibkr_notice, ErrorSeverity::information,
                        ErrorScope::historical_data_farm, false, id,
                        timestamp_ms, code, message, advanced_reject_json);
    case 2158:
      return make_error(ErrorCode::ibkr_notice, ErrorSeverity::information,
                        ErrorScope::security_definition_farm, false, id,
                        timestamp_ms, code, message, advanced_reject_json);
    case 2100:
      return make_error(ErrorCode::ibkr_warning, ErrorSeverity::warning,
                        ErrorScope::account, false, id, timestamp_ms, code,
                        message, advanced_reject_json);
    case 2101:
      return make_error(ErrorCode::request_failed, ErrorSeverity::error,
                        ErrorScope::account, true, id, timestamp_ms, code,
                        message, advanced_reject_json);
    case 2109:
    case 2137:
    case 2168:
    case 2169:
      return make_error(ErrorCode::ibkr_warning, ErrorSeverity::warning,
                        ErrorScope::order, false, id, timestamp_ms, code,
                        message, advanced_reject_json);
    default:
      break;
  }

  if (is_order_code(code) || !advanced_reject_json.empty()) {
    return make_error(ErrorCode::request_failed, ErrorSeverity::error,
                      ErrorScope::order, true, id, timestamp_ms, code, message,
                      advanced_reject_json);
  }

  if (id >= 0) {
    return make_error(ErrorCode::request_failed, ErrorSeverity::error,
                      ErrorScope::request, true, id, timestamp_ms, code,
                      message, advanced_reject_json);
  }

  return make_error(ErrorCode::ibkr_error, ErrorSeverity::error,
                    ErrorScope::general, false, id, timestamp_ms, code,
                    message, advanced_reject_json);
}

}  // namespace detail
}  // namespace ibapi4gu
