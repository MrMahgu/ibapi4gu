#pragma once

#include <string>

#include <ibapi4gu/error.hpp>

namespace ibapi4gu {
namespace detail {

Error classify_ibkr_error(int id, long long timestamp_ms, int code,
                          const std::string& message,
                          const std::string& advanced_reject_json);

}  // namespace detail
}  // namespace ibapi4gu
