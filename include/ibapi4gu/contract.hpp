#pragma once

#include <string>

namespace ibapi4gu {

struct Contract {
  int con_id;
  std::string symbol;
  std::string security_type;
  std::string last_trade_date_or_contract_month;
  double strike;
  std::string right;
  std::string multiplier;
  std::string exchange;
  std::string primary_exchange;
  std::string currency;
  std::string local_symbol;
  std::string trading_class;
  bool include_expired;
  std::string security_id_type;
  std::string security_id;
  std::string issuer_id;

  Contract() : con_id(0), strike(0.0), include_expired(false) {}
};

struct Stock : Contract {
  explicit Stock(const std::string& stock_symbol,
                 const std::string& stock_exchange = "SMART",
                 const std::string& stock_currency = "USD") {
    symbol = stock_symbol;
    security_type = "STK";
    exchange = stock_exchange;
    currency = stock_currency;
  }
};

struct ContractDetails {
  Contract contract;
  std::string market_name;
  double minimum_tick;
  std::string order_types;
  std::string valid_exchanges;
  int price_magnifier;
  int underlying_contract_id;
  std::string long_name;
  std::string contract_month;
  std::string industry;
  std::string category;
  std::string subcategory;
  std::string time_zone_id;
  std::string trading_hours;
  std::string liquid_hours;
  std::string market_rule_ids;
  std::string real_expiration_date;
  std::string last_trade_time;
  std::string stock_type;
  std::string cusip;
  std::string ratings;
  std::string bond_type;
  double coupon;
  std::string maturity;

  ContractDetails()
      : minimum_tick(0.0),
        price_magnifier(0),
        underlying_contract_id(0),
        coupon(0.0) {}
};

}  // namespace ibapi4gu
