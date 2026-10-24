#pragma once

#include <string>

#include <ibapi4gu/historical.hpp>

namespace ibapi4gu {
namespace detail {

std::string format_historical_duration(const HistoricalDuration& duration);
std::string format_historical_bar_size(HistoricalBarSize size);
std::string format_historical_data_type(HistoricalDataType type);
std::string format_historical_end_time(const HistoricalBarsOptions& options);

}  // namespace detail
}  // namespace ibapi4gu
