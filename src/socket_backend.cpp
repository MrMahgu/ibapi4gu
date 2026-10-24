#include "detail/backend.hpp"

#include <memory>
#include <mutex>

#include <Contract.h>
#include <EClientSocket.h>
#include <EReader.h>
#include <EReaderOSSignal.h>

namespace ibapi4gu {
namespace detail {
namespace {

::Contract to_ibkr_contract(const Contract& source) {
  ::Contract destination;
  destination.conId = source.con_id;
  destination.symbol = source.symbol;
  destination.secType = source.security_type;
  destination.lastTradeDateOrContractMonth =
      source.last_trade_date_or_contract_month;
  destination.strike = source.strike;
  destination.right = source.right;
  destination.multiplier = source.multiplier;
  destination.exchange = source.exchange;
  destination.primaryExchange = source.primary_exchange;
  destination.currency = source.currency;
  destination.localSymbol = source.local_symbol;
  destination.tradingClass = source.trading_class;
  destination.includeExpired = source.include_expired;
  destination.secIdType = source.security_id_type;
  destination.secId = source.security_id;
  destination.issuerId = source.issuer_id;
  return destination;
}

class SocketBackend : public Backend {
 public:
  SocketBackend() {}
  ~SocketBackend() override { disconnect(); }

  bool connect(const SessionOptions& options, EWrapper* wrapper,
               Error& error) override {
    try {
      const std::shared_ptr<EReaderOSSignal> signal(
          new EReaderOSSignal(25));
      {
        std::lock_guard<std::mutex> lock(signal_mutex_);
        signal_ = signal;
      }
      client_.reset(new EClientSocket(wrapper, signal.get()));
      if (!client_->eConnect(options.host.c_str(), options.port,
                             options.client_id, options.extra_auth)) {
        error = Error(ErrorCode::connection_failed,
                      "IBKR socket handshake failed");
        return false;
      }
      reader_.reset(new EReader(client_.get(), signal.get()));
      reader_->start();
      return true;
    } catch (const std::exception& exception) {
      error = Error(ErrorCode::connection_failed, exception.what());
      disconnect();
      return false;
    }
  }

  void poll() override {
    if (!signal_ || !reader_) {
      return;
    }
    signal_->waitForSignal();
    reader_->processMsgs();
  }

  void wake() override {
    std::shared_ptr<EReaderOSSignal> signal;
    {
      std::lock_guard<std::mutex> lock(signal_mutex_);
      signal = signal_;
    }
    if (signal) {
      signal->issueSignal();
    }
  }

  void disconnect() override {
    if (client_) {
      client_->eDisconnect();
    }
    if (reader_) {
      reader_->stop();
    }
    reader_.reset();
    client_.reset();
    {
      std::lock_guard<std::mutex> lock(signal_mutex_);
      signal_.reset();
    }
  }

  bool connected() const override {
    return client_ && client_->isConnected() && client_->isSocketOK();
  }

  void request_contract_details(RequestId id,
                                const Contract& contract) override {
    if (client_) {
      client_->reqContractDetails(id.value(), to_ibkr_contract(contract));
    }
  }

  void cancel_contract_details(RequestId id) override {
    if (client_) {
      client_->cancelContractData(id.value());
    }
  }

  void raw_call(const std::function<void(EClient&)>& function) override {
    if (client_) {
      function(*client_);
    }
  }

 private:
  mutable std::mutex signal_mutex_;
  std::shared_ptr<EReaderOSSignal> signal_;
  std::unique_ptr<EClientSocket> client_;
  std::unique_ptr<EReader> reader_;
};

}  // namespace

std::unique_ptr<Backend> make_socket_backend() {
  return std::unique_ptr<Backend>(new SocketBackend());
}

}  // namespace detail
}  // namespace ibapi4gu
