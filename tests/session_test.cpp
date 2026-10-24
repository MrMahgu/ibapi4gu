#include <ibapi4gu/api.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <exception>
#include <future>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <Contract.h>
#include <ContractData.pb.h>
#include <Decimal.h>
#include <EClient.h>
#include <ETransport.h>
#include <EWrapper.h>
#include <bar.h>

#include "detail/backend.hpp"
#include "detail/callback_router.hpp"
#include "detail/historical_request.hpp"
#include "detail/operation_registry.hpp"

namespace {

using ibapi4gu::ContractDetails;
using ibapi4gu::ErrorCode;
using ibapi4gu::RequestId;
using ibapi4gu::Result;
using ibapi4gu::Session;

void require(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

class TestExecutor {
 public:
  TestExecutor() : stopping_(false), worker_([this] { run(); }) {}

  ~TestExecutor() {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      stopping_ = true;
    }
    condition_.notify_all();
    worker_.join();
  }

  ibapi4gu::CompletionExecutor function() {
    return [this](std::function<void()> task) {
      {
        std::lock_guard<std::mutex> lock(mutex_);
        tasks_.push_back(std::move(task));
      }
      condition_.notify_all();
    };
  }

  std::thread::id thread_id() const { return worker_.get_id(); }

 private:
  void run() {
    for (;;) {
      std::function<void()> task;
      {
        std::unique_lock<std::mutex> lock(mutex_);
        condition_.wait(lock, [this] {
          return stopping_ || !tasks_.empty();
        });
        if (stopping_ && tasks_.empty()) {
          return;
        }
        task = tasks_.front();
        tasks_.pop_front();
      }
      task();
    }
  }

  mutable std::mutex mutex_;
  std::condition_variable condition_;
  std::deque<std::function<void()> > tasks_;
  bool stopping_;
  std::thread worker_;
};

struct Controller {
  struct SentRequest {
    int id;
    std::string symbol;
  };

  struct SentHistoricalRequest {
    int id;
    std::string symbol;
    ibapi4gu::HistoricalBarsOptions options;
  };

  explicit Controller(bool ready = true)
      : auto_ready(ready),
        connected(false),
        throw_on_connect(false),
        throw_on_poll(false),
        throw_on_disconnect(false),
        wake_requested(false),
        wrapper(NULL),
        raw_calls(0) {}

  void emit(const std::function<void(EWrapper&)>& event) {
    {
      std::lock_guard<std::mutex> lock(mutex);
      events.push_back(event);
    }
    condition.notify_all();
  }

  int wait_for_request(std::size_t index) {
    std::unique_lock<std::mutex> lock(mutex);
    require(condition.wait_for(lock, std::chrono::seconds(2),
                               [this, index] {
                                 return requests.size() > index;
                               }),
            "timed out waiting for a request");
    return requests[index].id;
  }

  int wait_for_historical_request(std::size_t index) {
    std::unique_lock<std::mutex> lock(mutex);
    require(condition.wait_for(lock, std::chrono::seconds(2),
                               [this, index] {
                                 return historical_requests.size() > index;
                               }),
            "timed out waiting for a historical request");
    return historical_requests[index].id;
  }

  void wait_for_wrapper() {
    std::unique_lock<std::mutex> lock(mutex);
    require(condition.wait_for(lock, std::chrono::seconds(2),
                               [this] { return wrapper != NULL; }),
            "timed out waiting for backend connection");
  }

  void wait_for_cancellations(std::size_t count) {
    std::unique_lock<std::mutex> lock(mutex);
    require(condition.wait_for(lock, std::chrono::seconds(2),
                               [this, count] {
                                 return cancellations.size() >= count;
                               }),
            "timed out waiting for cancellation");
  }

  void wait_for_historical_cancellations(std::size_t count) {
    std::unique_lock<std::mutex> lock(mutex);
    require(condition.wait_for(lock, std::chrono::seconds(2),
                               [this, count] {
                                 return historical_cancellations.size() >=
                                        count;
                               }),
            "timed out waiting for historical cancellation");
  }

  bool auto_ready;
  std::atomic<bool> connected;
  bool throw_on_connect;
  std::atomic<bool> throw_on_poll;
  std::atomic<bool> throw_on_disconnect;
  bool wake_requested;
  EWrapper* wrapper;
  std::mutex mutex;
  std::condition_variable condition;
  std::deque<std::function<void(EWrapper&)> > events;
  std::vector<SentRequest> requests;
  std::vector<int> cancellations;
  std::vector<SentHistoricalRequest> historical_requests;
  std::vector<int> historical_cancellations;
  std::atomic<int> raw_calls;
};

class FakeTransport : public ETransport {
 public:
  int send(EMessage*) override { return 0; }
};

class FakeClient : public EClient {
 public:
  explicit FakeClient(EWrapper* wrapper)
      : EClient(wrapper, new FakeTransport()) {}

  bool eConnect(const char*, int, int, bool) override { return true; }
  void eDisconnect(bool = true) override {}
  int receive(char*, size_t) override { return 0; }
  bool isSocketOK() const override { return true; }

 protected:
  void prepareBufferImpl(std::ostream&) const override {}
  void prepareBuffer(std::ostream&) const override {}
  bool closeAndSend(std::string, unsigned = 0) override { return true; }
};

class FakeBackend : public ibapi4gu::detail::Backend {
 public:
  explicit FakeBackend(const std::shared_ptr<Controller>& controller)
      : controller_(controller) {}

  bool connect(const ibapi4gu::SessionOptions&, EWrapper* wrapper,
               ibapi4gu::Error&) override {
    if (controller_->throw_on_connect) {
      throw std::runtime_error("connect exploded");
    }
    client_.reset(new FakeClient(wrapper));
    {
      std::lock_guard<std::mutex> lock(controller_->mutex);
      controller_->wrapper = wrapper;
      controller_->connected.store(true);
    }
    controller_->condition.notify_all();
    if (controller_->auto_ready) {
      wrapper->nextValidId(500);
    }
    return true;
  }

  void poll() override {
    if (controller_->throw_on_poll.exchange(false)) {
      throw std::runtime_error("poll exploded");
    }
    std::function<void(EWrapper&)> event;
    {
      std::unique_lock<std::mutex> lock(controller_->mutex);
      controller_->condition.wait_for(
          lock, std::chrono::milliseconds(5), [this] {
            return controller_->wake_requested ||
                   !controller_->events.empty();
          });
      controller_->wake_requested = false;
      if (!controller_->events.empty()) {
        event = controller_->events.front();
        controller_->events.pop_front();
      }
    }
    if (event) {
      event(*controller_->wrapper);
    }
  }

  void wake() override {
    {
      std::lock_guard<std::mutex> lock(controller_->mutex);
      controller_->wake_requested = true;
    }
    controller_->condition.notify_all();
  }

  void disconnect() override {
    controller_->connected.store(false);
    wake();
    if (controller_->throw_on_disconnect.exchange(false)) {
      throw std::runtime_error("disconnect exploded");
    }
  }

  bool connected() const override { return controller_->connected.load(); }

  void request_contract_details(
      RequestId id, const ibapi4gu::Contract& contract) override {
    {
      std::lock_guard<std::mutex> lock(controller_->mutex);
      Controller::SentRequest request;
      request.id = id.value();
      request.symbol = contract.symbol;
      controller_->requests.push_back(request);
    }
    controller_->condition.notify_all();
  }

  void cancel_contract_details(RequestId id) override {
    {
      std::lock_guard<std::mutex> lock(controller_->mutex);
      controller_->cancellations.push_back(id.value());
    }
    controller_->condition.notify_all();
  }

  void request_historical_bars(
      RequestId id, const ibapi4gu::Contract& contract,
      const ibapi4gu::HistoricalBarsOptions& options) override {
    {
      std::lock_guard<std::mutex> lock(controller_->mutex);
      Controller::SentHistoricalRequest request;
      request.id = id.value();
      request.symbol = contract.symbol;
      request.options = options;
      controller_->historical_requests.push_back(request);
    }
    controller_->condition.notify_all();
  }

  void cancel_historical_bars(RequestId id) override {
    {
      std::lock_guard<std::mutex> lock(controller_->mutex);
      controller_->historical_cancellations.push_back(id.value());
    }
    controller_->condition.notify_all();
  }

  void raw_call(const std::function<void(EClient&)>& function) override {
    ++controller_->raw_calls;
    function(*client_);
  }

 private:
  std::shared_ptr<Controller> controller_;
  std::unique_ptr<FakeClient> client_;
};

struct Fixture {
  explicit Fixture(bool ready = true)
      : controller(new Controller(ready)) {}

  Result<std::shared_ptr<Session> > connect() {
    ibapi4gu::SessionOptions options;
    options.connect_timeout = std::chrono::seconds(2);
    return ibapi4gu::detail::SessionFactory::connect(
        options, std::unique_ptr<ibapi4gu::detail::Backend>(
                     new FakeBackend(controller)));
  }

  std::shared_ptr<Controller> controller;
};

::ContractDetails row(const std::string& symbol,
                      const std::string& name = std::string()) {
  ::ContractDetails details;
  details.contract.symbol = symbol;
  details.contract.secType = "STK";
  details.longName = name;
  return details;
}

::Bar historical_row(const std::string& time, double close,
                     const std::string& volume) {
  ::Bar bar;
  bar.time = time;
  bar.open = close - 1.0;
  bar.high = close + 1.0;
  bar.low = close - 2.0;
  bar.close = close;
  bar.volume = DecimalFunctions::stringToDecimal(volume);
  bar.wap = DecimalFunctions::stringToDecimal("123.456");
  bar.count = 17;
  return bar;
}

void test_generic_value_operation_and_wire_formatting() {
  const std::shared_ptr<ibapi4gu::detail::RequestState<int> > state(
      new ibapi4gu::detail::RequestState<int>());
  ibapi4gu::detail::OperationRegistry registry;
  const std::shared_ptr<ibapi4gu::detail::ValueOperation<int> > operation(
      new ibapi4gu::detail::ValueOperation<int>(
          RequestId(77),
          std::chrono::steady_clock::now() + std::chrono::seconds(1),
          std::function<void(RequestId)>(), state));
  require(registry.add(operation), "one-value operation was not registered");
  const std::shared_ptr<ibapi4gu::detail::ValueOperation<int> > taken =
      registry.take<ibapi4gu::detail::ValueOperation<int> >(RequestId(77));
  require(taken && registry.size() == 0,
          "one-value operation was not removed by type");
  taken->succeed(42);
  const Result<int> value = state->wait();
  require(value && value.value() == 42,
          "one-value operation did not complete its request state");

  require(ibapi4gu::detail::format_historical_duration(
              ibapi4gu::HistoricalDuration(
                  3, ibapi4gu::HistoricalDurationUnit::weeks)) == "3 W" &&
              ibapi4gu::detail::format_historical_bar_size(
                  ibapi4gu::HistoricalBarSize::eight_hours) == "8 hours" &&
              ibapi4gu::detail::format_historical_data_type(
                  ibapi4gu::HistoricalDataType::aggregate_trades) ==
                  "AGGTRADES",
          "typed historical options did not map to IBKR wire values");

  ibapi4gu::HistoricalBarsOptions current;
  require(ibapi4gu::detail::format_historical_end_time(current).empty(),
          "current historical end time was not empty");
  ibapi4gu::HistoricalBarsOptions epoch;
  epoch.end_at_current_time = false;
  epoch.end_time = std::chrono::system_clock::from_time_t(0);
  require(ibapi4gu::detail::format_historical_end_time(epoch) ==
              "19700101-00:00:00",
          "historical end time was not formatted as UTC");
}

void test_request_waiting_and_result_value_semantics() {
  std::vector<int> numbers;
  numbers.push_back(7);
  Result<std::vector<int> > original =
      Result<std::vector<int> >::success(numbers);
  Result<std::vector<int> > copy = original;
  copy.value()[0] = 11;
  require(original.value()[0] == 7 && copy.value()[0] == 11,
          "Result copies shared mutable value storage");

  Result<void> void_success = Result<void>::success();
  void_success.value();
  require(void_success.has_value(), "Result<void> did not represent success");
  Result<void> void_failure = Result<void>::failure(
      ibapi4gu::Error(ErrorCode::operation_failed, "failed"));
  require(!void_failure &&
              void_failure.error().code == ErrorCode::operation_failed,
          "Result<void> did not preserve an error");

  Fixture fixture;
  Result<std::shared_ptr<Session> > connected = fixture.connect();
  require(connected.has_value(), "session did not connect");
  std::shared_ptr<Session> session = connected.value();

  ibapi4gu::Request<std::vector<ContractDetails> > request =
      session->contract_details(ibapi4gu::Stock("WAIT"));
  const int id = fixture.controller->wait_for_request(0);
  require(request.valid() && !request.ready() &&
              !request.wait_for(std::chrono::milliseconds(10)),
          "pending request reported ready");

  std::thread::id session_thread;
  ibapi4gu::Request<void> session_thread_probe = session->ibkr().post(
      [&session_thread](EClient&) {
        session_thread = std::this_thread::get_id();
      });
  require(session_thread_probe.get().has_value(),
          "could not identify the session thread");

  std::mutex completion_mutex;
  std::condition_variable completion_condition;
  int completion_count = 0;
  std::thread::id completion_thread;
  TestExecutor completion_executor;
  require(request.on_complete(
              completion_executor.function(),
              [&completion_mutex, &completion_condition, &completion_count,
               &completion_thread](
                  const Result<std::vector<ContractDetails> >& completed) {
                require(completed && completed.value().empty(),
                        "completion callback received the wrong result");
                {
                  std::lock_guard<std::mutex> lock(completion_mutex);
                  completion_thread = std::this_thread::get_id();
                  ++completion_count;
                }
                completion_condition.notify_all();
                throw std::runtime_error("completion callback test");
              }),
          "completion callback was not registered");

  std::future<void> first_waiter = std::async(
      std::launch::async, [&request] { request.wait(); });
  std::future<void> second_waiter = std::async(
      std::launch::async, [&request] { request.wait(); });
  fixture.controller->emit(
      [id](EWrapper& wrapper) { wrapper.contractDetailsEnd(id); });
  first_waiter.get();
  second_waiter.get();
  require(request.ready(), "completed request did not report ready");
  require(request.on_complete(
              completion_executor.function(),
              [&completion_mutex, &completion_condition, &completion_count](
                  const Result<std::vector<ContractDetails> >& completed) {
                require(completed.has_value(),
                        "late completion callback received an error");
                {
                  std::lock_guard<std::mutex> lock(completion_mutex);
                  ++completion_count;
                }
                completion_condition.notify_all();
              }),
          "late completion callback was not registered");
  {
    std::unique_lock<std::mutex> lock(completion_mutex);
    require(completion_condition.wait_for(
                lock, std::chrono::seconds(2),
                [&completion_count] { return completion_count == 2; }),
            "completion callbacks did not execute");
  }
  require(completion_thread == completion_executor.thread_id() &&
              completion_thread != session_thread,
          "completion callback ran on the session thread");

  Result<std::vector<ContractDetails> > result;
  require(request.try_get(result) && result && result.value().empty(),
          "try_get did not consume a ready request");
  const Result<std::vector<ContractDetails> > consumed_result = request.get();
  const bool registered_after_consume = request.on_complete(
      completion_executor.function(),
      [](const Result<std::vector<ContractDetails> >&) {});
  require(!request.valid() && !request.try_get(result) &&
              !consumed_result &&
              consumed_result.error().code == ErrorCode::invalid_operation &&
              !registered_after_consume,
          "consumed request remained valid");

  ibapi4gu::Request<std::vector<ContractDetails> > cancelled =
      session->contract_details(ibapi4gu::Stock("CANCEL_WAIT"));
  fixture.controller->wait_for_request(1);
  std::future<Result<std::vector<ContractDetails> > > waiting_get =
      std::async(std::launch::async, [&cancelled] {
        return cancelled.get();
      });
  cancelled.cancel();
  cancelled.cancel();
  const Result<std::vector<ContractDetails> > cancelled_result =
      waiting_get.get();
  require(!cancelled_result &&
              cancelled_result.error().code == ErrorCode::cancelled,
          "cancel concurrent with get did not complete deterministically");
  fixture.controller->wait_for_cancellations(1);
  {
    std::lock_guard<std::mutex> lock(fixture.controller->mutex);
    require(fixture.controller->cancellations.size() == 1,
            "repeated cancellation reached the wire more than once");
  }
}

void test_observable_raw_operations() {
  Fixture fixture;
  Result<std::shared_ptr<Session> > connected = fixture.connect();
  require(connected.has_value(), "session did not connect");
  std::shared_ptr<Session> session = connected.value();
  ibapi4gu::IbkrGateway gateway = session->ibkr();

  std::atomic<int> executions(0);
  ibapi4gu::Request<void> success = gateway.post(
      [&executions](EClient&) { ++executions; });
  require(success.get().has_value() && executions.load() == 1,
          "raw operation did not report successful execution");

  ibapi4gu::Request<void> failure = gateway.post([](EClient&) {
    throw std::runtime_error("raw operation exploded");
  });
  const Result<void> failure_result = failure.get();
  require(!failure_result &&
              failure_result.error().code == ErrorCode::operation_failed &&
              failure_result.error().message == "raw operation exploded",
          "raw operation exception was not returned to its caller");

  gateway.post([&executions](EClient&) { ++executions; });
  ibapi4gu::Request<void> detached_fence = gateway.post([](EClient&) {});
  require(detached_fence.get().has_value() && executions.load() == 2,
          "ignoring a raw result cancelled fire-and-forget work");

  std::mutex blocker_mutex;
  std::condition_variable blocker_condition;
  bool blocker_started = false;
  bool release_blocker = false;
  ibapi4gu::Request<void> blocker = gateway.post(
      [&blocker_mutex, &blocker_condition, &blocker_started,
       &release_blocker](EClient&) {
        std::unique_lock<std::mutex> lock(blocker_mutex);
        blocker_started = true;
        blocker_condition.notify_all();
        blocker_condition.wait(lock, [&release_blocker] {
          return release_blocker;
        });
      });
  {
    std::unique_lock<std::mutex> lock(blocker_mutex);
    require(blocker_condition.wait_for(
                lock, std::chrono::seconds(2),
                [&blocker_started] { return blocker_started; }),
            "raw blocker did not start");
  }

  std::atomic<bool> cancelled_executed(false);
  ibapi4gu::Request<void> cancelled = gateway.post(
      [&cancelled_executed](EClient&) { cancelled_executed.store(true); });
  cancelled.cancel();
  require(cancelled.wait_for(std::chrono::milliseconds(100)),
          "cancelled raw operation did not become ready");
  const Result<void> cancelled_result = cancelled.get();
  require(!cancelled_result &&
              cancelled_result.error().code == ErrorCode::cancelled,
          "queued raw cancellation did not return a cancellation error");

  {
    std::lock_guard<std::mutex> lock(blocker_mutex);
    release_blocker = true;
  }
  blocker_condition.notify_all();
  require(blocker.get().has_value(), "raw blocker did not complete");
  ibapi4gu::Request<void> cancellation_fence = gateway.post([](EClient&) {});
  require(cancellation_fence.get().has_value() &&
              !cancelled_executed.load(),
          "cancelled raw operation executed on the session thread");

  session->shutdown();
  ibapi4gu::Request<void> rejected = gateway.post([](EClient&) {});
  const Result<void> rejected_result = rejected.get();
  require(!rejected_result &&
              rejected_result.error().code == ErrorCode::shutdown,
          "raw operation queued after shutdown did not fail observably");
}

void test_connection_waits_for_traditional_next_valid_id() {
  Fixture fixture(false);
  std::future<Result<std::shared_ptr<Session> > > future =
      std::async(std::launch::async, [&fixture] { return fixture.connect(); });

  protobuf::NextValidId proto;
  fixture.controller->emit(
      [proto](EWrapper& wrapper) { wrapper.nextValidIdProtoBuf(proto); });
  require(future.wait_for(std::chrono::milliseconds(75)) ==
              std::future_status::timeout,
          "Protobuf nextValidId made the session ready");

  fixture.controller->emit(
      [](EWrapper& wrapper) { wrapper.nextValidId(700); });
  Result<std::shared_ptr<Session> > result = future.get();
  require(result.has_value(), "traditional nextValidId did not connect");
  require(result.value()->reserve_order_id().value() == 700,
          "order allocator was not seeded from nextValidId");
  result.value()->shutdown();
}

void test_fatal_connection_error_finishes_connect() {
  const int fatal_codes[] = {326, 505, 506, 507, 508, 509, 520};
  for (std::size_t index = 0;
       index != sizeof(fatal_codes) / sizeof(fatal_codes[0]); ++index) {
    Fixture fixture(false);
    std::future<Result<std::shared_ptr<Session> > > future =
        std::async(std::launch::async,
                   [&fixture] { return fixture.connect(); });
    fixture.controller->wait_for_wrapper();
    const int code = fatal_codes[index];
    fixture.controller->emit([code](EWrapper& wrapper) {
      wrapper.error(-1, 456, code, "fatal connection failure", "");
    });

    require(future.wait_for(std::chrono::milliseconds(500)) ==
                std::future_status::ready,
            "fatal connection error waited for the connection timeout");
    const Result<std::shared_ptr<Session> > result = future.get();
    require(!result && result.error().code == ErrorCode::connection_failed &&
                result.error().severity == ibapi4gu::ErrorSeverity::fatal &&
                result.error().scope == ibapi4gu::ErrorScope::connection &&
                result.error().terminal && result.error().ibkr_code == code,
            "fatal connection error was not classified");
  }
}

void test_backend_exceptions_are_contained() {
  {
    Fixture fixture;
    fixture.controller->throw_on_connect = true;
    const Result<std::shared_ptr<Session> > result = fixture.connect();
    require(!result && result.error().code == ErrorCode::connection_failed &&
                result.error().scope == ibapi4gu::ErrorScope::connection &&
                result.error().terminal,
            "backend connect exception escaped the session result");
  }

  {
    Fixture fixture;
    Result<std::shared_ptr<Session> > connected = fixture.connect();
    require(connected.has_value(), "session did not connect");
    std::shared_ptr<Session> session = connected.value();
    ibapi4gu::Request<std::vector<ContractDetails> > request =
        session->contract_details(ibapi4gu::Stock("POLL"));
    fixture.controller->wait_for_request(0);
    fixture.controller->throw_on_poll.store(true);
    fixture.controller->emit([](EWrapper&) {});
    const Result<std::vector<ContractDetails> > result = request.get();
    require(!result && result.error().code == ErrorCode::backend_failure &&
                result.error().severity == ibapi4gu::ErrorSeverity::fatal &&
                result.error().scope == ibapi4gu::ErrorScope::connection &&
                result.error().terminal,
            "backend poll exception did not fail pending operations");
    session->shutdown();
  }

  {
    Fixture fixture;
    Result<std::shared_ptr<Session> > connected = fixture.connect();
    require(connected.has_value(), "session did not connect");
    std::shared_ptr<Session> session = connected.value();
    std::atomic<int> disconnect_errors(0);
    ibapi4gu::Subscription errors = session->on_error(
        [&disconnect_errors](const ibapi4gu::Error& error) {
          if (error.code == ErrorCode::backend_failure && !error.terminal) {
            ++disconnect_errors;
          }
        });
    fixture.controller->throw_on_disconnect.store(true);
    session->shutdown();
    require(session->state() == ibapi4gu::SessionState::disconnected &&
                disconnect_errors.load() == 1,
            "backend disconnect exception escaped shutdown");
  }
}

void test_concurrent_contract_details_are_correlated() {
  Fixture fixture;
  Result<std::shared_ptr<Session> > connected = fixture.connect();
  require(connected.has_value(), "session did not connect");
  std::shared_ptr<Session> session = connected.value();

  ibapi4gu::Request<std::vector<ContractDetails> > apple =
      session->contract_details(ibapi4gu::Stock("AAPL"));
  ibapi4gu::Request<std::vector<ContractDetails> > microsoft =
      session->contract_details(ibapi4gu::Stock("MSFT"));
  const int apple_id = fixture.controller->wait_for_request(0);
  const int microsoft_id = fixture.controller->wait_for_request(1);

  const ::ContractDetails apple_row = row("AAPL", "Apple");
  const ::ContractDetails microsoft_row = row("MSFT", "Microsoft");
  fixture.controller->emit([microsoft_id, microsoft_row](EWrapper& wrapper) {
    wrapper.contractDetails(microsoft_id, microsoft_row);
  });
  fixture.controller->emit([apple_id, apple_row](EWrapper& wrapper) {
    wrapper.contractDetails(apple_id, apple_row);
  });
  fixture.controller->emit([microsoft_id](EWrapper& wrapper) {
    wrapper.contractDetailsEnd(microsoft_id);
  });
  fixture.controller->emit([apple_id](EWrapper& wrapper) {
    wrapper.contractDetailsEnd(apple_id);
  });

  Result<std::vector<ContractDetails> > microsoft_result = microsoft.get();
  Result<std::vector<ContractDetails> > apple_result = apple.get();
  require(microsoft_result && microsoft_result.value().size() == 1 &&
              microsoft_result.value()[0].contract.symbol == "MSFT",
          "Microsoft result was cross-correlated");
  require(apple_result && apple_result.value().size() == 1 &&
              apple_result.value()[0].long_name == "Apple",
          "Apple result was cross-correlated");
}

void test_empty_error_timeout_cancel_abandon_and_late_callbacks() {
  Fixture fixture;
  Result<std::shared_ptr<Session> > connected = fixture.connect();
  require(connected.has_value(), "session did not connect");
  std::shared_ptr<Session> session = connected.value();

  ibapi4gu::Request<std::vector<ContractDetails> > empty =
      session->contract_details(ibapi4gu::Stock("EMPTY"));
  const int empty_id = fixture.controller->wait_for_request(0);
  fixture.controller->emit(
      [empty_id](EWrapper& wrapper) { wrapper.contractDetailsEnd(empty_id); });
  Result<std::vector<ContractDetails> > empty_result = empty.get();
  require(empty_result && empty_result.value().empty(),
          "empty result did not succeed");

  ibapi4gu::Request<std::vector<ContractDetails> > failed =
      session->contract_details(ibapi4gu::Stock("FAIL"));
  const int failed_id = fixture.controller->wait_for_request(1);
  fixture.controller->emit([failed_id](EWrapper& wrapper) {
    wrapper.error(failed_id, 123, 200, "request failed", "");
  });
  Result<std::vector<ContractDetails> > failed_result = failed.get();
  require(!failed_result &&
              failed_result.error().code == ErrorCode::request_failed &&
              failed_result.error().ibkr_code == 200,
          "matching IBKR error did not fail request");

  ibapi4gu::Request<std::vector<ContractDetails> > timed_out =
      session->contract_details(
          ibapi4gu::Stock("SLOW"),
          ibapi4gu::RequestOptions(std::chrono::milliseconds(20)));
  fixture.controller->wait_for_request(2);
  Result<std::vector<ContractDetails> > timeout_result = timed_out.get();
  require(!timeout_result && timeout_result.error().code == ErrorCode::timeout,
          "request timeout did not fail request");

  ibapi4gu::Request<std::vector<ContractDetails> > cancelled =
      session->contract_details(ibapi4gu::Stock("CANCEL"));
  const int cancelled_id = fixture.controller->wait_for_request(3);
  cancelled.cancel();
  Result<std::vector<ContractDetails> > cancelled_result = cancelled.get();
  require(!cancelled_result &&
              cancelled_result.error().code == ErrorCode::cancelled,
          "explicit cancellation did not fail request");

  const ::ContractDetails late = row("LATE");
  fixture.controller->emit([cancelled_id, late](EWrapper& wrapper) {
    wrapper.contractDetails(cancelled_id, late);
    wrapper.contractDetailsEnd(cancelled_id);
  });

  {
    ibapi4gu::Request<std::vector<ContractDetails> > abandoned =
        session->contract_details(ibapi4gu::Stock("ABANDON"));
    fixture.controller->wait_for_request(4);
  }
  fixture.controller->wait_for_cancellations(3);
}

void test_historical_bars_and_generic_operation_registry() {
  Fixture fixture;
  Result<std::shared_ptr<Session> > connected = fixture.connect();
  require(connected.has_value(), "session did not connect");
  std::shared_ptr<Session> session = connected.value();

  std::mutex error_mutex;
  std::condition_variable error_condition;
  std::vector<ibapi4gu::Error> errors;
  ibapi4gu::Subscription error_subscription = session->on_error(
      [&error_mutex, &error_condition, &errors](const ibapi4gu::Error& error) {
        std::lock_guard<std::mutex> lock(error_mutex);
        errors.push_back(error);
        error_condition.notify_all();
      });

  ibapi4gu::HistoricalBarsOptions options;
  options.duration = ibapi4gu::HistoricalDuration(
      3, ibapi4gu::HistoricalDurationUnit::days);
  options.bar_size = ibapi4gu::HistoricalBarSize::five_minutes;
  options.data_type = ibapi4gu::HistoricalDataType::midpoint;
  options.regular_trading_hours_only = false;

  ibapi4gu::Request<std::vector<ibapi4gu::HistoricalBar> > history =
      session->historical_bars(ibapi4gu::Stock("AAPL"), options);
  ibapi4gu::Request<std::vector<ContractDetails> > contract =
      session->contract_details(ibapi4gu::Stock("MSFT"));
  const int history_id = fixture.controller->wait_for_historical_request(0);
  const int contract_id = fixture.controller->wait_for_request(0);

  {
    std::lock_guard<std::mutex> lock(fixture.controller->mutex);
    const Controller::SentHistoricalRequest& sent =
        fixture.controller->historical_requests[0];
    require(sent.symbol == "AAPL" && sent.options.duration.count == 3 &&
                sent.options.bar_size ==
                    ibapi4gu::HistoricalBarSize::five_minutes &&
                sent.options.data_type ==
                    ibapi4gu::HistoricalDataType::midpoint &&
                !sent.options.regular_trading_hours_only,
            "historical request options were not preserved");
  }

  // Farm status notifications use the error channel but must not terminate a
  // request, even if a synthetic event happens to carry the same numeric ID.
  fixture.controller->emit([history_id](EWrapper& wrapper) {
    wrapper.error(history_id, 1000, 2106,
                  "historical data farm connection is OK", "");
  });
  fixture.controller->emit([history_id](EWrapper& wrapper) {
    wrapper.error(history_id, 1001, 201, "raw order rejected",
                  "{\"reason\":\"test\"}");
  });
  fixture.controller->emit([history_id](EWrapper& wrapper) {
    wrapper.error(history_id, 1002, 2109,
                  "order attribute ignored; order processed", "");
  });
  const ::Bar first = historical_row("1700000000", 125.0, "1000000000000");
  const ::Bar second = historical_row("1700000300", 126.0, "42.5");
  fixture.controller->emit([history_id, first](EWrapper& wrapper) {
    wrapper.historicalData(history_id, first);
  });
  fixture.controller->emit([contract_id](EWrapper& wrapper) {
    wrapper.contractDetailsEnd(contract_id);
  });
  fixture.controller->emit([history_id, second](EWrapper& wrapper) {
    wrapper.historicalData(history_id, second);
    wrapper.historicalDataEnd(history_id, "start", "end");
  });

  const Result<std::vector<ContractDetails> > contract_result = contract.get();
  const Result<std::vector<ibapi4gu::HistoricalBar> > history_result =
      history.get();
  require(contract_result && contract_result.value().empty(),
          "mixed operation type did not complete independently");
  require(history_result && history_result.value().size() == 2 &&
              history_result.value()[0].time == "1700000000" &&
              history_result.value()[0].close == 125.0 &&
              history_result.value()[0].volume.is_set() &&
              history_result.value()[0].volume.value() == "1000000000000" &&
              history_result.value()[1].weighted_average_price.value() ==
                  "123.456" &&
              history_result.value()[1].trade_count == 17,
          "historical bars were not normalized losslessly");

  {
    std::unique_lock<std::mutex> lock(error_mutex);
    require(error_condition.wait_for(lock, std::chrono::seconds(2), [&errors] {
              return !errors.empty();
            }),
            "farm status was not published");
    require(errors[0].code == ErrorCode::ibkr_notice &&
                errors[0].severity ==
                    ibapi4gu::ErrorSeverity::information &&
                errors[0].scope ==
                    ibapi4gu::ErrorScope::historical_data_farm &&
                !errors[0].terminal,
            "farm status was classified as a request failure");
    require(errors.size() >= 2 &&
                errors[1].scope == ibapi4gu::ErrorScope::order &&
                errors[1].terminal &&
                errors[1].advanced_reject_json ==
                    "{\"reason\":\"test\"}",
            "order error was not kept out of request correlation");
    require(errors.size() >= 3 &&
                errors[2].code == ErrorCode::ibkr_warning &&
                errors[2].severity == ibapi4gu::ErrorSeverity::warning &&
                errors[2].scope == ibapi4gu::ErrorScope::order &&
                !errors[2].terminal,
            "order warning was classified as a terminal error");
  }

  ibapi4gu::Request<std::vector<ibapi4gu::HistoricalBar> > failed =
      session->historical_bars(ibapi4gu::Stock("BAD"));
  const int failed_id = fixture.controller->wait_for_historical_request(1);
  fixture.controller->emit([failed_id](EWrapper& wrapper) {
    wrapper.error(failed_id, 1001, 162, "historical service error", "");
  });
  const Result<std::vector<ibapi4gu::HistoricalBar> > failed_result =
      failed.get();
  require(!failed_result &&
              failed_result.error().code == ErrorCode::request_failed &&
              failed_result.error().scope == ibapi4gu::ErrorScope::request &&
              failed_result.error().terminal,
          "terminal historical error did not fail its operation");

  ibapi4gu::Request<std::vector<ibapi4gu::HistoricalBar> > timed_out =
      session->historical_bars(
          ibapi4gu::Stock("SLOW"), ibapi4gu::HistoricalBarsOptions(),
          ibapi4gu::RequestOptions(std::chrono::milliseconds(20)));
  fixture.controller->wait_for_historical_request(2);
  const Result<std::vector<ibapi4gu::HistoricalBar> > timeout_result =
      timed_out.get();
  require(!timeout_result && timeout_result.error().code == ErrorCode::timeout,
          "historical timeout did not use generic operation expiry");

  ibapi4gu::Request<std::vector<ibapi4gu::HistoricalBar> > cancelled =
      session->historical_bars(ibapi4gu::Stock("CANCEL"));
  fixture.controller->wait_for_historical_request(3);
  cancelled.cancel();
  const Result<std::vector<ibapi4gu::HistoricalBar> > cancelled_result =
      cancelled.get();
  require(!cancelled_result &&
              cancelled_result.error().code == ErrorCode::cancelled,
          "historical cancellation did not use generic operation registry");
  fixture.controller->wait_for_historical_cancellations(2);

  ibapi4gu::HistoricalBarsOptions invalid_options;
  invalid_options.duration.count = 0;
  ibapi4gu::Request<std::vector<ibapi4gu::HistoricalBar> > invalid =
      session->historical_bars(ibapi4gu::Stock("INVALID"), invalid_options);
  const Result<std::vector<ibapi4gu::HistoricalBar> > invalid_result =
      invalid.get();
  require(!invalid_result &&
              invalid_result.error().code == ErrorCode::invalid_operation,
          "invalid historical duration was not rejected locally");
}

class CountingObserver : public ibapi4gu::detail::EWrapperAdapter {
 public:
  CountingObserver() : traditional(0), protobuf_count(0), current_times(0) {}

  void contractDetails(int, const ::ContractDetails&) override {
    ++traditional;
  }
  void contractDataProtoBuf(const protobuf::ContractData&) override {
    ++protobuf_count;
  }
  void currentTime(long long) override { ++current_times; }

  std::atomic<int> traditional;
  std::atomic<int> protobuf_count;
  std::atomic<int> current_times;
};

class ThrowingObserver : public ibapi4gu::detail::EWrapperAdapter {
 public:
  void currentTime(long long) override { throw std::runtime_error("observer"); }
};

void test_raw_callbacks_and_normalized_deduplication() {
  Fixture fixture;
  Result<std::shared_ptr<Session> > connected = fixture.connect();
  require(connected.has_value(), "session did not connect");
  std::shared_ptr<Session> session = connected.value();
  std::shared_ptr<CountingObserver> counting(new CountingObserver());
  std::shared_ptr<ThrowingObserver> throwing(new ThrowingObserver());
  ibapi4gu::Subscription first = session->ibkr().observe(throwing);
  ibapi4gu::Subscription second = session->ibkr().observe(counting);
  std::atomic<int> callback_errors(0);
  ibapi4gu::Subscription errors = session->on_error(
      [&callback_errors](const ibapi4gu::Error& error) {
        if (error.code == ErrorCode::callback_exception) {
          ++callback_errors;
        }
      });

  ibapi4gu::Request<std::vector<ContractDetails> > request =
      session->contract_details(ibapi4gu::Stock("ONE"));
  const int id = fixture.controller->wait_for_request(0);
  const protobuf::ContractData proto;
  const ::ContractDetails traditional = row("ONE");
  fixture.controller->emit([proto](EWrapper& wrapper) {
    wrapper.contractDataProtoBuf(proto);
  });
  fixture.controller->emit([id, traditional](EWrapper& wrapper) {
    wrapper.contractDetails(id, traditional);
    wrapper.contractDetailsEnd(id);
  });
  Result<std::vector<ContractDetails> > result = request.get();
  require(result && result.value().size() == 1,
          "Protobuf callback duplicated normalized row");
  const std::chrono::steady_clock::time_point routing_deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while ((counting->protobuf_count.load() != 1 ||
          counting->traditional.load() != 1) &&
         std::chrono::steady_clock::now() < routing_deadline) {
    std::this_thread::yield();
  }
  require(counting->protobuf_count.load() == 1 &&
              counting->traditional.load() == 1,
          "raw observer did not receive both callback families");

  fixture.controller->emit(
      [](EWrapper& wrapper) { wrapper.currentTime(1); });
  fixture.controller->emit(
      [](EWrapper& wrapper) { wrapper.currentTime(2); });
  const std::chrono::steady_clock::time_point deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (counting->current_times.load() != 2 &&
         std::chrono::steady_clock::now() < deadline) {
    std::this_thread::yield();
  }
  require(counting->current_times.load() == 2,
          "observer exception stopped callback processing");
  require(callback_errors.load() >= 2,
          "observer exception did not publish normalized errors");
}

void test_raw_queue_order_and_allocator_stress() {
  Fixture fixture;
  Result<std::shared_ptr<Session> > connected = fixture.connect();
  require(connected.has_value(), "session did not connect");
  std::shared_ptr<Session> session = connected.value();

  std::mutex order_mutex;
  std::condition_variable order_condition;
  std::vector<int> order;
  std::thread::id callback_thread;
  for (int value = 0; value != 20; ++value) {
    session->ibkr().post([&order_mutex, &order_condition, &order,
                          &callback_thread, value](EClient&) {
      std::lock_guard<std::mutex> lock(order_mutex);
      if (order.empty()) {
        callback_thread = std::this_thread::get_id();
      }
      order.push_back(value);
      order_condition.notify_all();
    });
  }
  {
    std::unique_lock<std::mutex> lock(order_mutex);
    require(order_condition.wait_for(lock, std::chrono::seconds(2), [&order] {
              return order.size() == 20;
            }),
            "timed out waiting for queued raw operations");
    for (int value = 0; value != 20; ++value) {
      require(order[static_cast<std::size_t>(value)] == value,
              "queued raw operations executed out of order");
    }
  }
  require(callback_thread != std::this_thread::get_id(),
          "raw operation did not execute on session thread");

  ibapi4gu::Request<std::vector<ContractDetails> > high_level =
      session->contract_details(ibapi4gu::Stock("IDS"));
  const int high_level_id = fixture.controller->wait_for_request(0);

  std::mutex ids_mutex;
  std::set<int> request_ids;
  std::set<int> order_ids;
  std::vector<std::thread> workers;
  for (int worker = 0; worker != 8; ++worker) {
    workers.push_back(std::thread([session, &ids_mutex, &request_ids,
                                   &order_ids] {
      for (int index = 0; index != 500; ++index) {
        const int request_id = session->reserve_request_id().value();
        const int order_id = session->reserve_order_id().value();
        std::lock_guard<std::mutex> lock(ids_mutex);
        request_ids.insert(request_id);
        order_ids.insert(order_id);
      }
    }));
  }
  for (std::vector<std::thread>::iterator iterator = workers.begin();
       iterator != workers.end(); ++iterator) {
    iterator->join();
  }
  require(request_ids.size() == 4000 && order_ids.size() == 4000,
          "concurrent ID allocation collided");
  require(request_ids.count(high_level_id) == 0,
          "raw and high-level request IDs collided");
  require(*request_ids.rbegin() - *request_ids.begin() == 3999 &&
              *order_ids.rbegin() - *order_ids.begin() == 3999,
          "ID allocators were not monotonic");

  fixture.controller->emit([high_level_id](EWrapper& wrapper) {
    wrapper.contractDetailsEnd(high_level_id);
  });
  require(high_level.get().has_value(),
          "allocator test request did not complete");
}

class ShutdownObserver : public ibapi4gu::detail::EWrapperAdapter {
 public:
  explicit ShutdownObserver(const std::shared_ptr<Session>& value)
      : session(value) {}
  void currentTime(long long) override { session->shutdown(); }
  std::shared_ptr<Session> session;
};

void test_disconnect_and_shutdown_from_callback() {
  {
    Fixture fixture;
    Result<std::shared_ptr<Session> > connected = fixture.connect();
    require(connected.has_value(), "session did not connect");
    std::shared_ptr<Session> session = connected.value();
    ibapi4gu::Request<std::vector<ContractDetails> > request =
        session->contract_details(ibapi4gu::Stock("DROP"));
    fixture.controller->wait_for_request(0);
    const std::shared_ptr<Controller> controller = fixture.controller;
    fixture.controller->emit([controller](EWrapper& wrapper) {
      controller->connected.store(false);
      wrapper.connectionClosed();
    });
    Result<std::vector<ContractDetails> > result = request.get();
    require(!result && result.error().code == ErrorCode::disconnected,
            "disconnect did not fail pending request");
  }

  {
    Fixture fixture;
    Result<std::shared_ptr<Session> > connected = fixture.connect();
    require(connected.has_value(), "session did not connect");
    std::shared_ptr<Session> session = connected.value();
    std::shared_ptr<ShutdownObserver> observer(new ShutdownObserver(session));
    ibapi4gu::Subscription subscription = session->ibkr().observe(observer);
    fixture.controller->emit(
        [](EWrapper& wrapper) { wrapper.currentTime(10); });
    const std::chrono::steady_clock::time_point deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (session->state() != ibapi4gu::SessionState::disconnected &&
           std::chrono::steady_clock::now() < deadline) {
      std::this_thread::yield();
    }
    require(session->state() == ibapi4gu::SessionState::disconnected,
            "shutdown from callback deadlocked");
    session->shutdown();
    ibapi4gu::Request<std::vector<ContractDetails> > after_shutdown =
        session->contract_details(ibapi4gu::Stock("TOO_LATE"));
    Result<std::vector<ContractDetails> > rejected = after_shutdown.get();
    require(!rejected && rejected.error().code == ErrorCode::disconnected,
            "request issued after shutdown did not complete");
  }
}

#define IBAPI4GU_COUNT_CALLBACK(name, parameters, arguments, tagged) +1
enum {
  regular_callback_count =
      0 IBAPI4GU_EWRAPPER_REGULAR_CALLBACKS(IBAPI4GU_COUNT_CALLBACK),
  proto_callback_count =
      0 IBAPI4GU_EWRAPPER_PROTO_CALLBACKS(IBAPI4GU_COUNT_CALLBACK)
};
#undef IBAPI4GU_COUNT_CALLBACK

static_assert(regular_callback_count == 89,
              "traditional callback manifest is incomplete");
static_assert(proto_callback_count == 81,
              "Protobuf callback manifest is incomplete");

}  // namespace

int main() {
  typedef void (*Test)();
  const std::pair<const char*, Test> tests[] = {
      std::make_pair("generic value operation",
                     &test_generic_value_operation_and_wire_formatting),
      std::make_pair("request waiting and result values",
                     &test_request_waiting_and_result_value_semantics),
      std::make_pair("observable raw operations",
                     &test_observable_raw_operations),
      std::make_pair("connection readiness",
                     &test_connection_waits_for_traditional_next_valid_id),
      std::make_pair("connection error classification",
                     &test_fatal_connection_error_finishes_connect),
      std::make_pair("backend exception containment",
                     &test_backend_exceptions_are_contained),
      std::make_pair("request correlation",
                     &test_concurrent_contract_details_are_correlated),
      std::make_pair("finite request outcomes",
                     &test_empty_error_timeout_cancel_abandon_and_late_callbacks),
      std::make_pair("historical operation registry",
                     &test_historical_bars_and_generic_operation_registry),
      std::make_pair("raw callback routing",
                     &test_raw_callbacks_and_normalized_deduplication),
      std::make_pair("raw queue and allocators",
                     &test_raw_queue_order_and_allocator_stress),
      std::make_pair("disconnect and callback shutdown",
                     &test_disconnect_and_shutdown_from_callback),
  };
  for (std::size_t index = 0; index != sizeof(tests) / sizeof(tests[0]);
       ++index) {
    try {
      tests[index].second();
    } catch (const std::exception& exception) {
      std::cerr << tests[index].first << ": " << exception.what() << '\n';
      return 1;
    }
  }
  return 0;
}
