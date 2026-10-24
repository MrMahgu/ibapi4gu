#pragma once

#include <chrono>
#include <string>

namespace ibapi4gu {

// Lossless textual representation of an IBKR decimal value. A default
// instance represents IBKR's unset decimal sentinel.
class DecimalValue {
 public:
  DecimalValue() : set_(false) {}
  explicit DecimalValue(const std::string& value) : set_(true), value_(value) {}

  bool is_set() const { return set_; }
  const std::string& value() const { return value_; }

  bool operator==(const DecimalValue& other) const {
    return set_ == other.set_ && value_ == other.value_;
  }
  bool operator!=(const DecimalValue& other) const {
    return !(*this == other);
  }

 private:
  bool set_;
  std::string value_;
};

enum class HistoricalDurationUnit { seconds, days, weeks, months, years };

struct HistoricalDuration {
  int count;
  HistoricalDurationUnit unit;

  HistoricalDuration()
      : count(1), unit(HistoricalDurationUnit::days) {}
  HistoricalDuration(int duration_count, HistoricalDurationUnit duration_unit)
      : count(duration_count), unit(duration_unit) {}
};

enum class HistoricalBarSize {
  one_second,
  five_seconds,
  ten_seconds,
  fifteen_seconds,
  thirty_seconds,
  one_minute,
  two_minutes,
  three_minutes,
  five_minutes,
  ten_minutes,
  fifteen_minutes,
  twenty_minutes,
  thirty_minutes,
  one_hour,
  two_hours,
  three_hours,
  four_hours,
  eight_hours,
  one_day,
  one_week,
  one_month
};

enum class HistoricalDataType {
  trades,
  midpoint,
  bid,
  ask,
  bid_ask,
  adjusted_last,
  aggregate_trades,
  historical_volatility,
  option_implied_volatility,
  fee_rate,
  yield_bid,
  yield_ask,
  yield_bid_ask,
  yield_last
};

struct HistoricalBarsOptions {
  bool end_at_current_time;
  std::chrono::system_clock::time_point end_time;
  HistoricalDuration duration;
  HistoricalBarSize bar_size;
  HistoricalDataType data_type;
  bool regular_trading_hours_only;

  HistoricalBarsOptions()
      : end_at_current_time(true),
        bar_size(HistoricalBarSize::one_minute),
        data_type(HistoricalDataType::trades),
        regular_trading_hours_only(true) {}
};

struct HistoricalBar {
  // IBKR returns epoch seconds for intraday bars and a calendar value for
  // day-or-longer bars. Preserve that losslessly until calendar semantics are
  // selected by the application.
  std::string time;
  double open;
  double high;
  double low;
  double close;
  DecimalValue volume;
  DecimalValue weighted_average_price;
  int trade_count;

  HistoricalBar()
      : open(0.0),
        high(0.0),
        low(0.0),
        close(0.0),
        trade_count(0) {}
};

}  // namespace ibapi4gu
