#include "detail/historical_request.hpp"

#include <ctime>
#include <sstream>
#include <stdexcept>

namespace ibapi4gu {
namespace detail {

std::string format_historical_duration(const HistoricalDuration& duration) {
  char unit = 'D';
  switch (duration.unit) {
    case HistoricalDurationUnit::seconds:
      unit = 'S';
      break;
    case HistoricalDurationUnit::days:
      unit = 'D';
      break;
    case HistoricalDurationUnit::weeks:
      unit = 'W';
      break;
    case HistoricalDurationUnit::months:
      unit = 'M';
      break;
    case HistoricalDurationUnit::years:
      unit = 'Y';
      break;
  }
  std::ostringstream stream;
  stream << duration.count << ' ' << unit;
  return stream.str();
}

std::string format_historical_bar_size(HistoricalBarSize size) {
  switch (size) {
    case HistoricalBarSize::one_second:
      return "1 secs";
    case HistoricalBarSize::five_seconds:
      return "5 secs";
    case HistoricalBarSize::ten_seconds:
      return "10 secs";
    case HistoricalBarSize::fifteen_seconds:
      return "15 secs";
    case HistoricalBarSize::thirty_seconds:
      return "30 secs";
    case HistoricalBarSize::one_minute:
      return "1 min";
    case HistoricalBarSize::two_minutes:
      return "2 mins";
    case HistoricalBarSize::three_minutes:
      return "3 mins";
    case HistoricalBarSize::five_minutes:
      return "5 mins";
    case HistoricalBarSize::ten_minutes:
      return "10 mins";
    case HistoricalBarSize::fifteen_minutes:
      return "15 mins";
    case HistoricalBarSize::twenty_minutes:
      return "20 mins";
    case HistoricalBarSize::thirty_minutes:
      return "30 mins";
    case HistoricalBarSize::one_hour:
      return "1 hour";
    case HistoricalBarSize::two_hours:
      return "2 hours";
    case HistoricalBarSize::three_hours:
      return "3 hours";
    case HistoricalBarSize::four_hours:
      return "4 hours";
    case HistoricalBarSize::eight_hours:
      return "8 hours";
    case HistoricalBarSize::one_day:
      return "1 day";
    case HistoricalBarSize::one_week:
      return "1 week";
    case HistoricalBarSize::one_month:
      return "1 month";
  }
  throw std::invalid_argument("unknown historical bar size");
}

std::string format_historical_data_type(HistoricalDataType type) {
  switch (type) {
    case HistoricalDataType::trades:
      return "TRADES";
    case HistoricalDataType::midpoint:
      return "MIDPOINT";
    case HistoricalDataType::bid:
      return "BID";
    case HistoricalDataType::ask:
      return "ASK";
    case HistoricalDataType::bid_ask:
      return "BID_ASK";
    case HistoricalDataType::adjusted_last:
      return "ADJUSTED_LAST";
    case HistoricalDataType::aggregate_trades:
      return "AGGTRADES";
    case HistoricalDataType::historical_volatility:
      return "HISTORICAL_VOLATILITY";
    case HistoricalDataType::option_implied_volatility:
      return "OPTION_IMPLIED_VOLATILITY";
    case HistoricalDataType::fee_rate:
      return "FEE_RATE";
    case HistoricalDataType::yield_bid:
      return "YIELD_BID";
    case HistoricalDataType::yield_ask:
      return "YIELD_ASK";
    case HistoricalDataType::yield_bid_ask:
      return "YIELD_BID_ASK";
    case HistoricalDataType::yield_last:
      return "YIELD_LAST";
  }
  throw std::invalid_argument("unknown historical data type");
}

std::string format_historical_end_time(
    const HistoricalBarsOptions& options) {
  if (options.end_at_current_time) {
    return std::string();
  }
  const std::time_t value =
      std::chrono::system_clock::to_time_t(options.end_time);
  std::tm utc;
#if defined(_WIN32)
  if (gmtime_s(&utc, &value) != 0) {
    throw std::runtime_error("could not format historical end time");
  }
#else
  if (gmtime_r(&value, &utc) == NULL) {
    throw std::runtime_error("could not format historical end time");
  }
#endif
  char buffer[32];
  if (std::strftime(buffer, sizeof(buffer), "%Y%m%d-%H:%M:%S", &utc) == 0) {
    throw std::runtime_error("could not format historical end time");
  }
  return std::string(buffer);
}

}  // namespace detail
}  // namespace ibapi4gu
