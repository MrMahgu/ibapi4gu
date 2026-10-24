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
#include <EClient.h>
#include <ETransport.h>
#include <EWrapper.h>

#include "detail/backend.hpp"
#include "detail/callback_router.hpp"

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

struct Controller {
  struct SentRequest {
    int id;
    std::string symbol;
  };

  explicit Controller(bool ready = true)
      : auto_ready(ready),
        connected(false),
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

  void wait_for_cancellations(std::size_t count) {
    std::unique_lock<std::mutex> lock(mutex);
    require(condition.wait_for(lock, std::chrono::seconds(2),
                               [this, count] {
                                 return cancellations.size() >= count;
                               }),
            "timed out waiting for cancellation");
  }

  bool auto_ready;
  std::atomic<bool> connected;
  bool wake_requested;
  EWrapper* wrapper;
  std::mutex mutex;
  std::condition_variable condition;
  std::deque<std::function<void(EWrapper&)> > events;
  std::vector<SentRequest> requests;
  std::vector<int> cancellations;
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
      std::make_pair("connection readiness",
                     &test_connection_waits_for_traditional_next_valid_id),
      std::make_pair("request correlation",
                     &test_concurrent_contract_details_are_correlated),
      std::make_pair("finite request outcomes",
                     &test_empty_error_timeout_cancel_abandon_and_late_callbacks),
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
