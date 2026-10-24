#pragma once

#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

#include <ibapi4gu/result.hpp>

namespace ibapi4gu {
class IbkrGateway;

typedef std::function<void(std::function<void()>)> CompletionExecutor;

namespace detail {

class SessionCore;

template <typename T>
class RequestState {
 public:
  RequestState() : ready_(false) {}

  bool complete(Result<T> result) {
    std::vector<Completion> completions;
    std::shared_ptr<const Result<T> > callback_result;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (ready_) {
        return false;
      }
      result_.reset(new Result<T>(std::move(result)));
      ready_ = true;
      completions.swap(completions_);
      if (!completions.empty()) {
        callback_result.reset(new Result<T>(*result_));
      }
    }
    condition_.notify_all();
    for (typename std::vector<Completion>::const_iterator iterator =
             completions.begin();
         iterator != completions.end(); ++iterator) {
      dispatch(*iterator, callback_result);
    }
    return true;
  }

  Result<T> wait() const {
    std::unique_lock<std::mutex> lock(mutex_);
    condition_.wait(lock, [this] { return ready_; });
    return *result_;
  }

  void wait_until_ready() const {
    std::unique_lock<std::mutex> lock(mutex_);
    condition_.wait(lock, [this] { return ready_; });
  }

  template <typename Rep, typename Period>
  bool wait_for(const std::chrono::duration<Rep, Period>& timeout) const {
    std::unique_lock<std::mutex> lock(mutex_);
    return condition_.wait_for(lock, timeout, [this] { return ready_; });
  }

  bool on_complete(
      const CompletionExecutor& executor,
      const std::function<void(const Result<T>&)>& callback) {
    if (!executor || !callback) {
      return false;
    }

    Completion completion;
    completion.executor = executor;
    completion.callback = callback;
    std::shared_ptr<const Result<T> > result;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (!ready_) {
        completions_.push_back(completion);
        return true;
      }
      result.reset(new Result<T>(*result_));
    }
    dispatch(completion, result);
    return true;
  }

  bool ready() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return ready_;
  }

 private:
  struct Completion {
    CompletionExecutor executor;
    std::function<void(const Result<T>&)> callback;
  };

  static void dispatch(
      const Completion& completion,
      const std::shared_ptr<const Result<T> >& result) {
    const std::function<void(const Result<T>&)> callback =
        completion.callback;
    try {
      completion.executor([callback, result] {
        try {
          callback(*result);
        } catch (...) {
        }
      });
    } catch (...) {
    }
  }

  mutable std::mutex mutex_;
  mutable std::condition_variable condition_;
  bool ready_;
  std::unique_ptr<Result<T> > result_;
  std::vector<Completion> completions_;
};

enum class AbandonPolicy { cancel, detach };

template <typename T>
class RequestControl {
 public:
  RequestControl(const std::shared_ptr<RequestState<T> >& state,
                 const std::function<void()>& cancel,
                 AbandonPolicy abandon_policy)
      : state_(state),
        cancel_(cancel),
        abandon_policy_(abandon_policy),
        get_started_(false),
        consumed_(false),
        cancellation_requested_(false) {}

  bool valid() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_ && !consumed_;
  }

  std::shared_ptr<RequestState<T> > state() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return consumed_ ? std::shared_ptr<RequestState<T> >() : state_;
  }

  bool begin_get(std::shared_ptr<RequestState<T> >& state) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!state_ || consumed_ || get_started_) {
      return false;
    }
    get_started_ = true;
    state = state_;
    return true;
  }

  void finish_get() {
    std::lock_guard<std::mutex> lock(mutex_);
    consumed_ = true;
    cancel_ = std::function<void()>();
    state_.reset();
  }

  void cancel() {
    std::function<void()> cancel;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (!state_ || consumed_ || cancellation_requested_ ||
          state_->ready()) {
        return;
      }
      cancellation_requested_ = true;
      cancel = cancel_;
    }
    if (cancel) {
      cancel();
    }
  }

  void abandon() {
    std::function<void()> cancel;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (!state_ || consumed_) {
        return;
      }
      if (abandon_policy_ == AbandonPolicy::cancel && !get_started_ &&
          !cancellation_requested_ && !state_->ready()) {
        cancellation_requested_ = true;
        cancel = cancel_;
      }
      consumed_ = true;
      cancel_ = std::function<void()>();
      state_.reset();
    }
    if (cancel) {
      cancel();
    }
  }

 private:
  mutable std::mutex mutex_;
  std::shared_ptr<RequestState<T> > state_;
  std::function<void()> cancel_;
  AbandonPolicy abandon_policy_;
  bool get_started_;
  bool consumed_;
  bool cancellation_requested_;
};

}  // namespace detail

template <typename T>
class Request {
 public:
  Request() {}

  Request(Request&& other) noexcept
      : control_(std::move(other.control_)) {}

  Request& operator=(Request&& other) noexcept {
    if (this != &other) {
      abandon();
      control_ = std::move(other.control_);
    }
    return *this;
  }

  ~Request() { abandon(); }

  Request(const Request&) = delete;
  Request& operator=(const Request&) = delete;

  bool valid() const { return control_ && control_->valid(); }

  bool ready() const {
    const std::shared_ptr<detail::RequestState<T> > state = request_state();
    return state && state->ready();
  }

  void wait() const {
    const std::shared_ptr<detail::RequestState<T> > state = request_state();
    if (state) {
      state->wait_until_ready();
    }
  }

  template <typename Rep, typename Period>
  bool wait_for(const std::chrono::duration<Rep, Period>& timeout) const {
    const std::shared_ptr<detail::RequestState<T> > state = request_state();
    return state && state->wait_for(timeout);
  }

  bool on_complete(
      const CompletionExecutor& executor,
      const std::function<void(const Result<T>&)>& callback) const {
    const std::shared_ptr<detail::RequestState<T> > state = request_state();
    return state && state->on_complete(executor, callback);
  }

  void cancel() {
    if (control_) {
      control_->cancel();
    }
  }

  Result<T> get() {
    std::shared_ptr<detail::RequestState<T> > state;
    if (!control_ || !control_->begin_get(state)) {
      return Result<T>::failure(
          Error(ErrorCode::invalid_operation, "request is not valid"));
    }
    try {
      Result<T> result = state->wait();
      control_->finish_get();
      return result;
    } catch (...) {
      control_->finish_get();
      throw;
    }
  }

  bool try_get(Result<T>& result) {
    const std::shared_ptr<detail::RequestState<T> > current = request_state();
    if (!current || !current->ready()) {
      return false;
    }
    std::shared_ptr<detail::RequestState<T> > state;
    if (!control_->begin_get(state)) {
      return false;
    }
    try {
      result = state->wait();
      control_->finish_get();
    } catch (...) {
      control_->finish_get();
      throw;
    }
    return true;
  }

 private:
  friend class Session;
  friend class IbkrGateway;
  friend class detail::SessionCore;

  Request(const std::shared_ptr<detail::RequestState<T> >& state,
          const std::function<void()>& cancel,
          detail::AbandonPolicy abandon_policy =
              detail::AbandonPolicy::cancel)
      : control_(new detail::RequestControl<T>(state, cancel,
                                               abandon_policy)) {}

  std::shared_ptr<detail::RequestState<T> > request_state() const {
    return control_ ? control_->state()
                    : std::shared_ptr<detail::RequestState<T> >();
  }

  void abandon() {
    if (control_) {
      control_->abandon();
    }
    control_.reset();
  }

  std::shared_ptr<detail::RequestControl<T> > control_;
};

}  // namespace ibapi4gu
