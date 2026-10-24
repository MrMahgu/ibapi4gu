#pragma once

#include <exception>
#include <memory>
#include <string>
#include <vector>

#include <EWrapper.h>

#include "ewrapper_manifest.inc"

namespace ibapi4gu {
namespace detail {

class RouterSink {
 public:
  virtual ~RouterSink() {}
  virtual void on_callback(const char* name) = 0;
  virtual void on_next_valid_id(int order_id) = 0;
  virtual void on_contract_details(int request_id,
                                   const ::ContractDetails& details) = 0;
  virtual void on_contract_details_end(int request_id) = 0;
  virtual void on_error(int id, long long timestamp_ms, int code,
                        const std::string& message,
                        const std::string& advanced_reject_json) = 0;
  virtual void on_connection_closed() = 0;
  virtual std::vector<std::shared_ptr<EWrapper> > raw_observers() = 0;
  virtual void on_observer_exception(const char* callback,
                                     const std::string& message) = 0;
};

class EWrapperAdapter : public EWrapper {
 public:
#define IBAPI4GU_DECLARE_NOOP(name, parameters, arguments, tagged) \
  void name parameters override { ignore arguments; }
  IBAPI4GU_EWRAPPER_CALLBACKS(IBAPI4GU_DECLARE_NOOP)
#undef IBAPI4GU_DECLARE_NOOP

 private:
  template <typename... Arguments>
  static void ignore(Arguments&&...) {}
};

class CallbackRouter : public EWrapper {
 public:
  explicit CallbackRouter(RouterSink& sink) : sink_(sink) {}

#define IBAPI4GU_DECLARE_CALLBACK(name, parameters, arguments, tagged) \
  void name parameters override;
  IBAPI4GU_EWRAPPER_CALLBACKS(IBAPI4GU_DECLARE_CALLBACK)
#undef IBAPI4GU_DECLARE_CALLBACK

 private:
#define IBAPI4GU_DECLARE_TAG(name, parameters, arguments, tagged) \
  struct name##_tag {                                               \
    static const char* callback_name() { return #name; }            \
  };
  IBAPI4GU_EWRAPPER_CALLBACKS(IBAPI4GU_DECLARE_TAG)
#undef IBAPI4GU_DECLARE_TAG

  template <typename Tag, typename... Arguments>
  void internal(Tag, Arguments&&...) {
    sink_.on_callback(Tag::callback_name());
  }

  void internal(nextValidId_tag, int order_id);
  void internal(contractDetails_tag, int request_id,
                const ::ContractDetails& details);
  void internal(bondContractDetails_tag, int request_id,
                const ::ContractDetails& details);
  void internal(contractDetailsEnd_tag, int request_id);
  void internal(error_tag, int id, long long timestamp_ms, int code,
                const std::string& message,
                const std::string& advanced_reject_json);
  void internal(connectionClosed_tag);

  template <typename Function>
  void forward(const char* callback, Function function) {
    const std::vector<std::shared_ptr<EWrapper> > observers =
        sink_.raw_observers();
    for (std::vector<std::shared_ptr<EWrapper> >::const_iterator iterator =
             observers.begin();
         iterator != observers.end(); ++iterator) {
      try {
        function(**iterator);
      } catch (const std::exception& exception) {
        sink_.on_observer_exception(callback, exception.what());
      } catch (...) {
        sink_.on_observer_exception(callback, "unknown exception");
      }
    }
  }

  RouterSink& sink_;
};

}  // namespace detail
}  // namespace ibapi4gu
