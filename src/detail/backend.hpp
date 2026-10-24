#pragma once

#include <functional>
#include <memory>

#include <ibapi4gu/contract.hpp>
#include <ibapi4gu/error.hpp>
#include <ibapi4gu/historical.hpp>
#include <ibapi4gu/result.hpp>
#include <ibapi4gu/session.hpp>
#include <ibapi4gu/types.hpp>

class EClient;
class EWrapper;

namespace ibapi4gu {
namespace detail {

class Backend {
 public:
  virtual ~Backend() {}

  virtual bool connect(const SessionOptions& options, EWrapper* wrapper,
                       Error& error) = 0;
  virtual void poll() = 0;
  virtual void wake() = 0;
  virtual void disconnect() = 0;
  virtual bool connected() const = 0;

  virtual void request_contract_details(RequestId id,
                                        const Contract& contract) = 0;
  virtual void cancel_contract_details(RequestId id) = 0;
  virtual void request_historical_bars(
      RequestId id, const Contract& contract,
      const HistoricalBarsOptions& options) = 0;
  virtual void cancel_historical_bars(RequestId id) = 0;
  virtual void raw_call(const std::function<void(EClient&)>& function) = 0;
};

class SessionFactory {
 public:
  static Result<std::shared_ptr<Session> > connect(
      const SessionOptions& options, std::unique_ptr<Backend> backend);
};

std::unique_ptr<Backend> make_socket_backend();

}  // namespace detail
}  // namespace ibapi4gu
