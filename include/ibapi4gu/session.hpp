#pragma once

#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include <ibapi4gu/contract.hpp>
#include <ibapi4gu/historical.hpp>
#include <ibapi4gu/request.hpp>
#include <ibapi4gu/result.hpp>
#include <ibapi4gu/types.hpp>

class EClient;
class EWrapper;

namespace ibapi4gu {
namespace detail {
class SessionCore;
class SessionFactory;
}

class Subscription {
 public:
  Subscription();
  Subscription(Subscription&& other) noexcept;
  Subscription& operator=(Subscription&& other) noexcept;
  ~Subscription();

  Subscription(const Subscription&) = delete;
  Subscription& operator=(const Subscription&) = delete;

  void reset();
  bool active() const;

 private:
  friend class Session;
  friend class IbkrGateway;
  friend class detail::SessionCore;
  explicit Subscription(const std::function<void()>& unsubscribe);

  std::function<void()> unsubscribe_;
};

class IbkrGateway {
 public:
  IbkrGateway();

  template <typename Function>
  void post(Function function) const {
    post_impl(std::function<void(EClient&)>(std::move(function)));
  }

  Subscription observe(const std::shared_ptr<EWrapper>& observer) const;

 private:
  friend class Session;
  explicit IbkrGateway(const std::weak_ptr<detail::SessionCore>& core);
  void post_impl(const std::function<void(EClient&)>& function) const;

  std::weak_ptr<detail::SessionCore> core_;
};

class Session : public std::enable_shared_from_this<Session> {
 public:
  static Result<std::shared_ptr<Session> > connect(
      const SessionOptions& options = SessionOptions());

  ~Session();

  Session(const Session&) = delete;
  Session& operator=(const Session&) = delete;

  SessionState state() const;
  RequestId reserve_request_id();
  OrderId reserve_order_id();

  Request<std::vector<ContractDetails> > contract_details(
      const Contract& contract,
      const RequestOptions& options = RequestOptions());

  Request<std::vector<HistoricalBar> > historical_bars(
      const Contract& contract,
      const HistoricalBarsOptions& historical_options =
          HistoricalBarsOptions(),
      const RequestOptions& request_options = RequestOptions());

  Subscription on_state(
      const std::function<void(SessionState)>& listener);
  Subscription on_error(const std::function<void(const Error&)>& listener);

  IbkrGateway ibkr();
  void shutdown();

 private:
  friend class detail::SessionCore;
  friend class detail::SessionFactory;
  explicit Session(const std::shared_ptr<detail::SessionCore>& core);

  std::shared_ptr<detail::SessionCore> core_;
};

}  // namespace ibapi4gu
