#pragma once

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>

#include <ibapi4gu/result.hpp>

namespace ibapi4gu {
namespace detail {

class SessionCore;

template <typename T>
class RequestState {
 public:
  RequestState() : ready_(false) {}

  bool complete(const Result<T>& result) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (ready_) {
      return false;
    }
    result_.reset(new Result<T>(result));
    ready_ = true;
    condition_.notify_all();
    return true;
  }

  Result<T> wait() {
    std::unique_lock<std::mutex> lock(mutex_);
    condition_.wait(lock, [this] { return ready_; });
    return *result_;
  }

  bool ready() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return ready_;
  }

 private:
  mutable std::mutex mutex_;
  std::condition_variable condition_;
  bool ready_;
  std::unique_ptr<Result<T> > result_;
};

}  // namespace detail

template <typename T>
class Request {
 public:
  Request() : consumed_(false) {}

  Request(Request&& other) noexcept
      : state_(std::move(other.state_)),
        cancel_(std::move(other.cancel_)),
        consumed_(other.consumed_) {
    other.consumed_ = true;
  }

  Request& operator=(Request&& other) noexcept {
    if (this != &other) {
      abandon();
      state_ = std::move(other.state_);
      cancel_ = std::move(other.cancel_);
      consumed_ = other.consumed_;
      other.consumed_ = true;
    }
    return *this;
  }

  ~Request() { abandon(); }

  Request(const Request&) = delete;
  Request& operator=(const Request&) = delete;

  bool valid() const { return static_cast<bool>(state_); }

  void cancel() {
    if (state_ && cancel_) {
      cancel_();
    }
  }

  Result<T> get() {
    if (!state_) {
      return Result<T>::failure(
          Error(ErrorCode::invalid_operation, "request is not valid"));
    }
    Result<T> result = state_->wait();
    consumed_ = true;
    cancel_ = std::function<void()>();
    state_.reset();
    return result;
  }

 private:
  friend class Session;
  friend class detail::SessionCore;

  Request(const std::shared_ptr<detail::RequestState<T> >& state,
          const std::function<void()>& cancel)
      : state_(state), cancel_(cancel), consumed_(false) {}

  void abandon() {
    if (state_ && !consumed_ && cancel_) {
      cancel_();
    }
    state_.reset();
    cancel_ = std::function<void()>();
    consumed_ = true;
  }

  std::shared_ptr<detail::RequestState<T> > state_;
  std::function<void()> cancel_;
  bool consumed_;
};

}  // namespace ibapi4gu
