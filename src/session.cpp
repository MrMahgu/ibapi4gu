#include <ibapi4gu/session.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <map>
#include <mutex>
#include <sstream>
#include <thread>
#include <utility>

#include <Contract.h>
#include <EClient.h>

#include "detail/backend.hpp"
#include "detail/callback_router.hpp"

namespace ibapi4gu {
namespace detail {
namespace {

Contract from_ibkr_contract(const ::Contract& source) {
  Contract destination;
  destination.con_id = source.conId;
  destination.symbol = source.symbol;
  destination.security_type = source.secType;
  destination.last_trade_date_or_contract_month =
      source.lastTradeDateOrContractMonth;
  destination.strike = source.strike;
  destination.right = source.right;
  destination.multiplier = source.multiplier;
  destination.exchange = source.exchange;
  destination.primary_exchange = source.primaryExchange;
  destination.currency = source.currency;
  destination.local_symbol = source.localSymbol;
  destination.trading_class = source.tradingClass;
  destination.include_expired = source.includeExpired;
  destination.security_id_type = source.secIdType;
  destination.security_id = source.secId;
  destination.issuer_id = source.issuerId;
  return destination;
}

ContractDetails from_ibkr_details(const ::ContractDetails& source) {
  ContractDetails destination;
  destination.contract = from_ibkr_contract(source.contract);
  destination.market_name = source.marketName;
  destination.minimum_tick = source.minTick;
  destination.order_types = source.orderTypes;
  destination.valid_exchanges = source.validExchanges;
  destination.price_magnifier = source.priceMagnifier;
  destination.underlying_contract_id = source.underConId;
  destination.long_name = source.longName;
  destination.contract_month = source.contractMonth;
  destination.industry = source.industry;
  destination.category = source.category;
  destination.subcategory = source.subcategory;
  destination.time_zone_id = source.timeZoneId;
  destination.trading_hours = source.tradingHours;
  destination.liquid_hours = source.liquidHours;
  destination.market_rule_ids = source.marketRuleIds;
  destination.real_expiration_date = source.realExpirationDate;
  destination.last_trade_time = source.lastTradeTime;
  destination.stock_type = source.stockType;
  destination.cusip = source.cusip;
  destination.ratings = source.ratings;
  destination.bond_type = source.bondType;
  destination.coupon = source.coupon;
  destination.maturity = source.maturity;
  return destination;
}

}  // namespace

class SessionCore : public RouterSink,
                    public std::enable_shared_from_this<SessionCore> {
 public:
  explicit SessionCore(std::unique_ptr<Backend> backend)
      : backend_(std::move(backend)),
        router_(*this),
        state_(SessionState::connecting),
        next_request_id_(1),
        next_order_id_(0),
        next_subscription_id_(1),
        stop_requested_(false),
        unexpected_disconnect_(false),
        connect_finished_(false),
        join_claimed_(false) {}

  ~SessionCore() { shutdown(true); }

  void start() {
    const std::shared_ptr<SessionCore> self = shared_from_this();
    thread_ = std::thread([self] { self->event_loop(); });
  }

  Result<std::shared_ptr<Session> > wait_until_ready(
      const SessionOptions& options,
      const std::shared_ptr<Session>& session) {
    std::unique_lock<std::mutex> lock(connect_mutex_);
    if (!connect_condition_.wait_for(
            lock, options.connect_timeout,
            [this] { return connect_finished_; })) {
      lock.unlock();
      shutdown(false);
      return Result<std::shared_ptr<Session> >::failure(
          Error(ErrorCode::connection_timeout,
                "timed out waiting for IBKR nextValidId"));
    }
    if (connect_error_.code != ErrorCode::none) {
      const Error error = connect_error_;
      lock.unlock();
      shutdown(true);
      return Result<std::shared_ptr<Session> >::failure(error);
    }
    return Result<std::shared_ptr<Session> >::success(session);
  }

  SessionState state() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return state_;
  }

  RequestId reserve_request_id() {
    return RequestId(next_request_id_.fetch_add(1));
  }

  OrderId reserve_order_id() { return OrderId(next_order_id_.fetch_add(1)); }

  Request<std::vector<ContractDetails> > contract_details(
      const Contract& contract, const RequestOptions& options) {
    const RequestId id = reserve_request_id();
    typedef std::vector<ContractDetails> Value;
    const std::shared_ptr<RequestState<Value> > state(
        new RequestState<Value>());
    const std::weak_ptr<SessionCore> weak = shared_from_this();
    if (this->state() != SessionState::ready) {
      state->complete(Result<Value>::failure(
          Error(ErrorCode::disconnected, "session is not ready", 0, 0,
                id.value())));
      return Request<Value>(state, std::function<void()>());
    }
    post([weak, id, contract, options, state] {
      const std::shared_ptr<SessionCore> self = weak.lock();
      if (!self) {
        state->complete(Result<Value>::failure(
            Error(ErrorCode::shutdown, "session no longer exists", 0, 0,
                  id.value())));
        return;
      }
      if (self->state() != SessionState::ready) {
        state->complete(Result<Value>::failure(
            Error(ErrorCode::disconnected, "session is not ready", 0, 0,
                  id.value())));
        return;
      }
      PendingContract pending;
      pending.state = state;
      pending.deadline = std::chrono::steady_clock::now() + options.timeout;
      self->pending_contracts_[id.value()] = pending;
      try {
        self->backend_->request_contract_details(id, contract);
      } catch (const std::exception& exception) {
        self->complete_contract_error(
            id.value(), Error(ErrorCode::request_failed, exception.what(), 0,
                              0, id.value()));
      } catch (...) {
        self->complete_contract_error(
            id.value(), Error(ErrorCode::request_failed,
                              "unknown exception while sending request", 0,
                              0, id.value()));
      }
    });

    const std::function<void()> cancel = [weak, id] {
      const std::shared_ptr<SessionCore> self = weak.lock();
      if (self) {
        self->cancel_contract_request(id);
      }
    };
    return Request<Value>(state, cancel);
  }

  Subscription add_state_listener(
      const std::function<void(SessionState)>& listener) {
    const std::size_t id = next_subscription_id_.fetch_add(1);
    {
      std::lock_guard<std::mutex> lock(listeners_mutex_);
      state_listeners_[id] = listener;
    }
    const std::weak_ptr<SessionCore> weak = shared_from_this();
    return Subscription([weak, id] {
      const std::shared_ptr<SessionCore> self = weak.lock();
      if (self) {
        std::lock_guard<std::mutex> lock(self->listeners_mutex_);
        self->state_listeners_.erase(id);
      }
    });
  }

  Subscription add_error_listener(
      const std::function<void(const Error&)>& listener) {
    const std::size_t id = next_subscription_id_.fetch_add(1);
    {
      std::lock_guard<std::mutex> lock(listeners_mutex_);
      error_listeners_[id] = listener;
    }
    const std::weak_ptr<SessionCore> weak = shared_from_this();
    return Subscription([weak, id] {
      const std::shared_ptr<SessionCore> self = weak.lock();
      if (self) {
        std::lock_guard<std::mutex> lock(self->listeners_mutex_);
        self->error_listeners_.erase(id);
      }
    });
  }

  Subscription add_observer(const std::shared_ptr<EWrapper>& observer) {
    if (!observer) {
      return Subscription();
    }
    const std::size_t id = next_subscription_id_.fetch_add(1);
    {
      std::lock_guard<std::mutex> lock(observers_mutex_);
      observers_[id] = observer;
    }
    const std::weak_ptr<SessionCore> weak = shared_from_this();
    return Subscription([weak, id] {
      const std::shared_ptr<SessionCore> self = weak.lock();
      if (self) {
        std::lock_guard<std::mutex> lock(self->observers_mutex_);
        self->observers_.erase(id);
      }
    });
  }

  void post_raw(const std::function<void(EClient&)>& function) {
    const std::weak_ptr<SessionCore> weak = shared_from_this();
    post([weak, function] {
      const std::shared_ptr<SessionCore> self = weak.lock();
      if (!self || self->state() != SessionState::ready) {
        if (self) {
          self->publish_error(Error(ErrorCode::disconnected,
                                    "raw operation rejected: session is not ready"));
        }
        return;
      }
      try {
        self->backend_->raw_call(function);
      } catch (const std::exception& exception) {
        self->publish_error(Error(ErrorCode::callback_exception,
                                  exception.what()));
      } catch (...) {
        self->publish_error(Error(ErrorCode::callback_exception,
                                  "raw operation threw an unknown exception"));
      }
    });
  }

  void shutdown(bool wait) {
    stop_requested_.store(true);
    backend_->wake();

    bool join = false;
    {
      std::lock_guard<std::mutex> lock(join_mutex_);
      if (thread_.joinable() && !join_claimed_) {
        join_claimed_ = true;
        if (!wait || std::this_thread::get_id() == thread_.get_id()) {
          thread_.detach();
        } else {
          join = true;
        }
      }
    }
    if (join) {
      thread_.join();
    }
  }

  void on_callback(const char*) override {}

  void on_next_valid_id(int order_id) override {
    int current = next_order_id_.load();
    while (current < order_id &&
           !next_order_id_.compare_exchange_weak(current, order_id)) {
    }
    if (state() == SessionState::connecting) {
      set_state(SessionState::ready);
      finish_connect(Error());
    }
  }

  void on_contract_details(int request_id,
                           const ::ContractDetails& details) override {
    std::map<int, PendingContract>::iterator iterator =
        pending_contracts_.find(request_id);
    if (iterator != pending_contracts_.end()) {
      iterator->second.rows.push_back(from_ibkr_details(details));
    }
  }

  void on_contract_details_end(int request_id) override {
    std::map<int, PendingContract>::iterator iterator =
        pending_contracts_.find(request_id);
    if (iterator == pending_contracts_.end()) {
      return;
    }
    iterator->second.state->complete(
        Result<std::vector<ContractDetails> >::success(
            std::move(iterator->second.rows)));
    pending_contracts_.erase(iterator);
  }

  void on_error(int id, long long timestamp_ms, int code,
                const std::string& message,
                const std::string&) override {
    const Error error(ErrorCode::request_failed, message, code, timestamp_ms,
                      id);
    if (id >= 0) {
      complete_contract_error(id, error);
    }
    publish_error(error);
  }

  void on_connection_closed() override {
    if (!stop_requested_.load()) {
      unexpected_disconnect_ = true;
      stop_requested_.store(true);
      const Error error(ErrorCode::disconnected,
                        "IBKR connection closed unexpectedly");
      fail_all_pending(error);
      set_state(SessionState::disconnected);
      finish_connect(error);
      publish_error(error);
    }
    backend_->wake();
  }

  std::vector<std::shared_ptr<EWrapper> > raw_observers() override {
    std::vector<std::shared_ptr<EWrapper> > result;
    std::lock_guard<std::mutex> lock(observers_mutex_);
    for (std::map<std::size_t, std::shared_ptr<EWrapper> >::const_iterator
             iterator = observers_.begin();
         iterator != observers_.end(); ++iterator) {
      result.push_back(iterator->second);
    }
    return result;
  }

  void on_observer_exception(const char* callback,
                             const std::string& message) override {
    std::ostringstream stream;
    stream << "raw observer threw from " << callback << ": " << message;
    publish_error(Error(ErrorCode::callback_exception, stream.str()));
  }

 private:
  struct PendingContract {
    std::shared_ptr<RequestState<std::vector<ContractDetails> > > state;
    std::vector<ContractDetails> rows;
    std::chrono::steady_clock::time_point deadline;
  };

  void event_loop() {
    Error error;
    if (!backend_->connect(options_, &router_, error)) {
      if (error.code == ErrorCode::none) {
        error = Error(ErrorCode::connection_failed,
                      "IBKR socket handshake failed");
      }
      set_state(SessionState::disconnected);
      finish_connect(error);
      return;
    }

    while (!stop_requested_.load()) {
      drain_tasks();
      if (stop_requested_.load()) {
        break;
      }
      backend_->poll();
      if (stop_requested_.load()) {
        break;
      }
      drain_tasks();
      expire_requests();
      if (!stop_requested_.load() && !backend_->connected()) {
        on_connection_closed();
      }
    }

    if (!unexpected_disconnect_ && state() != SessionState::disconnected) {
      set_state(SessionState::disconnecting);
    }
    // Complete work that was queued just before shutdown. These tasks observe
    // the non-ready state and resolve their handles without sending new work.
    drain_tasks();
    fail_all_pending(Error(ErrorCode::shutdown, "session shut down"));
    backend_->disconnect();
    set_state(SessionState::disconnected);
    finish_connect(Error(ErrorCode::shutdown,
                         "session shut down before nextValidId"));
  }

 public:
  void configure(const SessionOptions& options) { options_ = options; }

 private:
  void post(const std::function<void()>& task) {
    {
      std::lock_guard<std::mutex> lock(tasks_mutex_);
      tasks_.push_back(task);
    }
    backend_->wake();
  }

  void drain_tasks() {
    std::deque<std::function<void()> > tasks;
    {
      std::lock_guard<std::mutex> lock(tasks_mutex_);
      tasks.swap(tasks_);
    }
    while (!tasks.empty()) {
      const std::function<void()> task = tasks.front();
      tasks.pop_front();
      try {
        task();
      } catch (const std::exception& exception) {
        publish_error(
            Error(ErrorCode::callback_exception, exception.what()));
      } catch (...) {
        publish_error(Error(ErrorCode::callback_exception,
                            "queued operation threw an unknown exception"));
      }
    }
  }

  void cancel_contract_request(RequestId id) {
    const std::weak_ptr<SessionCore> weak = shared_from_this();
    post([weak, id] {
      const std::shared_ptr<SessionCore> self = weak.lock();
      if (!self) {
        return;
      }
      std::map<int, PendingContract>::iterator iterator =
          self->pending_contracts_.find(id.value());
      if (iterator == self->pending_contracts_.end()) {
        return;
      }
      try {
        self->backend_->cancel_contract_details(id);
      } catch (...) {
      }
      iterator->second.state->complete(
          Result<std::vector<ContractDetails> >::failure(
              Error(ErrorCode::cancelled, "request cancelled", 0, 0,
                    id.value())));
      self->pending_contracts_.erase(iterator);
    });
  }

  void expire_requests() {
    const std::chrono::steady_clock::time_point now =
        std::chrono::steady_clock::now();
    std::map<int, PendingContract>::iterator iterator =
        pending_contracts_.begin();
    while (iterator != pending_contracts_.end()) {
      if (iterator->second.deadline > now) {
        ++iterator;
        continue;
      }
      const int id = iterator->first;
      try {
        backend_->cancel_contract_details(RequestId(id));
      } catch (...) {
      }
      iterator->second.state->complete(
          Result<std::vector<ContractDetails> >::failure(
              Error(ErrorCode::timeout, "request timed out", 0, 0, id)));
      pending_contracts_.erase(iterator++);
    }
  }

  void complete_contract_error(int id, const Error& error) {
    std::map<int, PendingContract>::iterator iterator =
        pending_contracts_.find(id);
    if (iterator == pending_contracts_.end()) {
      return;
    }
    iterator->second.state->complete(
        Result<std::vector<ContractDetails> >::failure(error));
    pending_contracts_.erase(iterator);
  }

  void fail_all_pending(const Error& error) {
    for (std::map<int, PendingContract>::iterator iterator =
             pending_contracts_.begin();
         iterator != pending_contracts_.end(); ++iterator) {
      iterator->second.state->complete(
          Result<std::vector<ContractDetails> >::failure(error));
    }
    pending_contracts_.clear();
  }

  void set_state(SessionState value) {
    {
      std::lock_guard<std::mutex> lock(state_mutex_);
      if (state_ == value) {
        return;
      }
      state_ = value;
    }
    std::vector<std::function<void(SessionState)> > listeners;
    {
      std::lock_guard<std::mutex> lock(listeners_mutex_);
      for (std::map<std::size_t,
                    std::function<void(SessionState)> >::const_iterator
               iterator = state_listeners_.begin();
           iterator != state_listeners_.end(); ++iterator) {
        listeners.push_back(iterator->second);
      }
    }
    for (std::vector<std::function<void(SessionState)> >::const_iterator
             iterator = listeners.begin();
         iterator != listeners.end(); ++iterator) {
      try {
        (*iterator)(value);
      } catch (const std::exception& exception) {
        publish_error(
            Error(ErrorCode::callback_exception, exception.what()));
      } catch (...) {
        publish_error(Error(ErrorCode::callback_exception,
                            "state listener threw an unknown exception"));
      }
    }
  }

  void publish_error(const Error& error) {
    std::vector<std::function<void(const Error&)> > listeners;
    {
      std::lock_guard<std::mutex> lock(listeners_mutex_);
      for (std::map<std::size_t,
                    std::function<void(const Error&)> >::const_iterator
               iterator = error_listeners_.begin();
           iterator != error_listeners_.end(); ++iterator) {
        listeners.push_back(iterator->second);
      }
    }
    for (std::vector<std::function<void(const Error&)> >::const_iterator
             iterator = listeners.begin();
         iterator != listeners.end(); ++iterator) {
      try {
        (*iterator)(error);
      } catch (...) {
      }
    }
  }

  void finish_connect(const Error& error) {
    std::lock_guard<std::mutex> lock(connect_mutex_);
    if (connect_finished_) {
      return;
    }
    connect_error_ = error;
    connect_finished_ = true;
    connect_condition_.notify_all();
  }

  std::unique_ptr<Backend> backend_;
  CallbackRouter router_;
  SessionOptions options_;

  mutable std::mutex state_mutex_;
  SessionState state_;
  std::atomic<int> next_request_id_;
  std::atomic<int> next_order_id_;
  std::atomic<std::size_t> next_subscription_id_;

  std::mutex listeners_mutex_;
  std::map<std::size_t, std::function<void(SessionState)> > state_listeners_;
  std::map<std::size_t, std::function<void(const Error&)> > error_listeners_;

  std::mutex observers_mutex_;
  std::map<std::size_t, std::shared_ptr<EWrapper> > observers_;

  std::mutex tasks_mutex_;
  std::deque<std::function<void()> > tasks_;
  std::map<int, PendingContract> pending_contracts_;

  std::atomic<bool> stop_requested_;
  bool unexpected_disconnect_;

  std::mutex connect_mutex_;
  std::condition_variable connect_condition_;
  bool connect_finished_;
  Error connect_error_;

  std::thread thread_;
  std::mutex join_mutex_;
  bool join_claimed_;
};

Result<std::shared_ptr<Session> > SessionFactory::connect(
    const SessionOptions& options, std::unique_ptr<Backend> backend) {
  if (!backend) {
    return Result<std::shared_ptr<Session> >::failure(
        Error(ErrorCode::connection_failed, "session backend is null"));
  }
  const std::shared_ptr<SessionCore> core(
      new SessionCore(std::move(backend)));
  core->configure(options);
  const std::shared_ptr<Session> session(new Session(core));
  core->start();
  return core->wait_until_ready(options, session);
}

}  // namespace detail

Subscription::Subscription() {}

Subscription::Subscription(const std::function<void()>& unsubscribe)
    : unsubscribe_(unsubscribe) {}

Subscription::Subscription(Subscription&& other) noexcept
    : unsubscribe_(std::move(other.unsubscribe_)) {
  other.unsubscribe_ = std::function<void()>();
}

Subscription& Subscription::operator=(Subscription&& other) noexcept {
  if (this != &other) {
    reset();
    unsubscribe_ = std::move(other.unsubscribe_);
    other.unsubscribe_ = std::function<void()>();
  }
  return *this;
}

Subscription::~Subscription() { reset(); }

void Subscription::reset() {
  if (!unsubscribe_) {
    return;
  }
  const std::function<void()> unsubscribe = unsubscribe_;
  unsubscribe_ = std::function<void()>();
  try {
    unsubscribe();
  } catch (...) {
  }
}

bool Subscription::active() const { return static_cast<bool>(unsubscribe_); }

IbkrGateway::IbkrGateway() {}

IbkrGateway::IbkrGateway(const std::weak_ptr<detail::SessionCore>& core)
    : core_(core) {}

void IbkrGateway::post_impl(
    const std::function<void(EClient&)>& function) const {
  const std::shared_ptr<detail::SessionCore> core = core_.lock();
  if (core) {
    core->post_raw(function);
  }
}

Subscription IbkrGateway::observe(
    const std::shared_ptr<EWrapper>& observer) const {
  const std::shared_ptr<detail::SessionCore> core = core_.lock();
  return core ? core->add_observer(observer) : Subscription();
}

Result<std::shared_ptr<Session> > Session::connect(
    const SessionOptions& options) {
  return detail::SessionFactory::connect(options,
                                         detail::make_socket_backend());
}

Session::Session(const std::shared_ptr<detail::SessionCore>& core)
    : core_(core) {}

Session::~Session() { shutdown(); }

SessionState Session::state() const {
  return core_ ? core_->state() : SessionState::disconnected;
}

RequestId Session::reserve_request_id() {
  return core_ ? core_->reserve_request_id() : RequestId();
}

OrderId Session::reserve_order_id() {
  return core_ ? core_->reserve_order_id() : OrderId();
}

Request<std::vector<ContractDetails> > Session::contract_details(
    const Contract& contract, const RequestOptions& options) {
  return core_ ? core_->contract_details(contract, options)
               : Request<std::vector<ContractDetails> >();
}

Subscription Session::on_state(
    const std::function<void(SessionState)>& listener) {
  return core_ ? core_->add_state_listener(listener) : Subscription();
}

Subscription Session::on_error(
    const std::function<void(const Error&)>& listener) {
  return core_ ? core_->add_error_listener(listener) : Subscription();
}

IbkrGateway Session::ibkr() { return IbkrGateway(core_); }

void Session::shutdown() {
  if (core_) {
    core_->shutdown(true);
  }
}

}  // namespace ibapi4gu
