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

  Result(const Result& other)
      : value_(other.value_ ? new T(*other.value_) : nullptr),
        error_(other.error_) {}

  Result& operator=(const Result& other) {
    if (this != &other) {
      Result copy(other);
      swap(copy);
    }
    return *this;
  }

  Result(Result&& other) noexcept
      : value_(std::move(other.value_)), error_(std::move(other.error_)) {}

  Result& operator=(Result&& other) noexcept {
    if (this != &other) {
      value_ = std::move(other.value_);
      error_ = std::move(other.error_);
    }
    return *this;
  }

  static Result success(T value) {
    Result result;
    result.value_.reset(new T(std::move(value)));
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

  void swap(Result& other) noexcept {
    value_.swap(other.value_);
    using std::swap;
    swap(error_, other.error_);
  }

 private:
  std::unique_ptr<T> value_;
  Error error_;
};

template <>
class Result<void> {
 public:
  Result()
      : has_value_(false),
        error_(ErrorCode::invalid_operation, "result has no value") {}

  static Result success() {
    Result result;
    result.has_value_ = true;
    result.error_ = Error();
    return result;
  }

  static Result failure(const Error& error) {
    Result result;
    result.error_ = error;
    return result;
  }

  explicit operator bool() const { return has_value_; }
  bool has_value() const { return has_value_; }

  void value() const {
    if (!has_value_) {
      throw std::logic_error("ibapi4gu::Result does not contain a value");
    }
  }

  const Error& error() const { return error_; }

 private:
  bool has_value_;
  Error error_;
};

}  // namespace ibapi4gu
