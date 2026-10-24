# Shared session API

`ibapi4gu::Session` adds a normalized, C++11-compatible layer without
replacing IBKR's socket, reader, decoder, or automatic traditional/Protobuf
wire-format selection. Link `ibapi4gu::api` for this layer. The existing
`ibapi4gu::twsapi` target remains the unmodified raw IBKR library.

## Connection and ownership

`Session::connect()` is synchronous. Success means both the socket handshake
and the traditional `nextValidId` callback have completed. A socket failure,
disconnect, or connection timeout returns an `Error` instead of a session.
Reconnection is manual: create a new session. Requests are never replayed.

A session owns one `EClientSocket`, `EReaderOSSignal`, `EReader`, and event-loop
thread. `shutdown()` is idempotent and may be called from any thread, including
a listener or raw callback. Destruction also shuts the session down.

## Threading

All normalized requests and `session->ibkr().post(...)` operations are queued
onto the event-loop thread. Callbacks are dispatched serially. Work posted
from a callback runs only after that callback returns. Listener and raw
observer exceptions are contained; raw observer exceptions are reported to
normalized error listeners and do not stop dispatch.

Do not block the event-loop thread. In particular, do not call
`Request<T>::get()` from a state listener, error listener, raw observer, or a
function passed to `ibkr().post()`.

## IDs

Request and order IDs have separate atomic, monotonic allocators. The order
allocator starts at the value supplied by `nextValidId`. Both normalized and
raw code must obtain IDs from `reserve_request_id()` or `reserve_order_id()`;
inventing IDs can collide with work sharing the same session.

## Finite requests

`contract_details()` returns a move-only request. `get()` blocks and returns a
`Result<std::vector<ContractDetails>>`. Its timeout defaults to 30 seconds.
Explicit `cancel()`, timeout, a matching IBKR error, disconnect, or shutdown
completes the result with an `Error`. Destroying a request before `get()`
cancels it. Callbacks arriving after completion are ignored.

## Raw interoperability

Use `session->ibkr().post([](EClient& client) { ... })` for serialized raw
outbound calls. Register a `std::shared_ptr<EWrapper>` with
`session->ibkr().observe(...)`; the RAII `Subscription` removes it.

Raw observers receive every traditional and Protobuf callback exactly as IBKR
delivers it. Internal correlation runs before observers. The normalized layer
consumes only traditional callbacks because IBKR can emit a Protobuf callback
and its converted traditional counterpart for the same message.

The Unix and Windows generated Protobuf trees are separate build profiles.
Their generated symbols overlap and their runtime versions differ, so two
profiles cannot be linked into or loaded by one process.
