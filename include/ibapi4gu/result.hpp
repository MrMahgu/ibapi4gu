#pragma once

#include <memory>
#include <stdexcept>
#include <utility>

#include <ibapi4gu/error.hpp>

namespace ibapi4gu {

template <typename T>
class Result {
 public:
  Result()
      : error_(ErrorCode::invalid_operation, "result has no value") {}

  static Result success(T value) {
    Result result;
    result.value_ = std::make_shared<T>(std::move(value));
    result.error_ = Error();
    return result;
  }

  static Result failure(const Error& error) {
    Result result;
    result.error_ = error;
    return result;
  }

  explicit operator bool() const { return static_cast<bool>(value_); }
  bool has_value() const { return static_cast<bool>(value_); }

  T& value() {
    if (!value_) {
      throw std::logic_error("ibapi4gu::Result does not contain a value");
    }
    return *value_;
  }

  const T& value() const {
    if (!value_) {
      throw std::logic_error("ibapi4gu::Result does not contain a value");
    }
    return *value_;
  }

  const Error& error() const { return error_; }

 private:
  std::shared_ptr<T> value_;
  Error error_;
};

}  // namespace ibapi4gu
