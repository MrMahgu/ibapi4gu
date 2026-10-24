#include "detail/callback_router.hpp"

namespace ibapi4gu {
namespace detail {

#define IBAPI4GU_DEFINE_CALLBACK(name, parameters, arguments, tagged)       \
  void CallbackRouter::name parameters {                                   \
    internal tagged;                                                       \
    forward(#name, [&](EWrapper& observer) { observer.name arguments; });  \
  }
IBAPI4GU_EWRAPPER_CALLBACKS(IBAPI4GU_DEFINE_CALLBACK)
#undef IBAPI4GU_DEFINE_CALLBACK

void CallbackRouter::internal(nextValidId_tag, int order_id) {
  sink_.on_callback("nextValidId");
  sink_.on_next_valid_id(order_id);
}

void CallbackRouter::internal(contractDetails_tag, int request_id,
                              const ::ContractDetails& details) {
  sink_.on_callback("contractDetails");
  sink_.on_contract_details(request_id, details);
}

void CallbackRouter::internal(bondContractDetails_tag, int request_id,
                              const ::ContractDetails& details) {
  sink_.on_callback("bondContractDetails");
  sink_.on_contract_details(request_id, details);
}

void CallbackRouter::internal(contractDetailsEnd_tag, int request_id) {
  sink_.on_callback("contractDetailsEnd");
  sink_.on_contract_details_end(request_id);
}

void CallbackRouter::internal(error_tag, int id, long long timestamp_ms,
                              int code, const std::string& message,
                              const std::string& advanced_reject_json) {
  sink_.on_callback("error");
  sink_.on_error(id, timestamp_ms, code, message, advanced_reject_json);
}

void CallbackRouter::internal(connectionClosed_tag) {
  sink_.on_callback("connectionClosed");
  sink_.on_connection_closed();
}

}  // namespace detail
}  // namespace ibapi4gu
