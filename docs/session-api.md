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

A `CompletionExecutor` accepts a `std::function<void()>` and must schedule it
on an application-owned execution context. `Request<T>::on_complete()` invokes
the executor but never directly invokes the completion callback. An inline
executor therefore violates this contract. Executor and completion-callback
exceptions are contained.

## IDs

Request and order IDs have separate atomic, monotonic allocators. The order
allocator starts at the value supplied by `nextValidId`. Both normalized and
raw code must obtain IDs from `reserve_request_id()` or `reserve_order_id()`;
inventing IDs can collide with work sharing the same session.

## Finite requests

`contract_details()` and `historical_bars()` return move-only requests. A
request provides:

- `ready()` for a nonblocking readiness check;
- `wait()` and `wait_for()` without consuming the result;
- `try_get(result)` to consume a result only when it is ready;
- `get()` to wait for and consume the result; and
- `on_complete(executor, callback)` to observe completion without blocking.

Multiple threads may wait concurrently, but exactly one `get()` or successful
`try_get()` consumes a request. Later retrieval returns `invalid_operation`.
`cancel()` is idempotent and may race with `get()`; the first terminal event
processed by the session wins, and `get()` returns that result. The timeout
defaults to 30 seconds. Explicit cancellation, timeout, a matching terminal
IBKR request error, disconnect, or shutdown completes the result with an
`Error`. Destroying a pending normalized request before retrieval cancels it.
Callbacks arriving after completion are ignored.

`Result<void>` represents successful operations without a payload. Copies of
`Result<T>` have independent value storage; mutating one successful result
does not mutate another copy.

Finite operations share one internal registry for request IDs, deadlines,
cancellation, failure, disconnect, and late-callback suppression. A typed
operation accepts only its own callback rows, so concurrent operation types
cannot cross-correlate even if a callback contains an unexpected ID.

`historical_bars()` is deliberately finite: it always sends
`keepUpToDate=false`. `HistoricalBarsOptions` uses typed duration, bar-size,
and data-type values. An empty end time requests data ending at the current
moment; a supplied `std::chrono::system_clock::time_point` is sent in UTC.
Volume and weighted-average price use `DecimalValue`, a lossless textual
representation that does not expose IBKR's decimal implementation. Intraday
bar times are returned as epoch text and day-or-longer bar times retain the
calendar value supplied by IBKR.

Historical data availability and pacing remain IBKR/account dependent. This
operation does not retry automatically.

## Error classification

IBKR multiplexes failures, warnings, connectivity changes, and normal farm
status notifications through `EWrapper::error`. `Error` preserves the IBKR
code, timestamp, related ID, and advanced order-reject JSON, and adds:

- `severity`: information, warning, error, or fatal;
- `scope`: request, order, connection, account, or a specific data farm; and
- `terminal`: whether the event ends work in that scope.

Only terminal request-scoped errors fail a correlated finite request. Market,
historical, and security-definition farm status messages are published to
error listeners but do not complete requests. Fatal connection errors fail
connection establishment immediately rather than waiting for the connect
timeout. Backend connection exceptions become fatal `connection_failed`
results, polling exceptions become fatal connection-scoped `backend_failure`
results, and disconnect cleanup exceptions are published as non-terminal
backend errors. The classifier does not perform automatic reconnect or replay.

## Raw interoperability

Use `session->ibkr().post([](EClient& client) { ... })` for serialized raw
outbound calls. It returns `Request<void>`: success means the function ran,
and queue rejection or an exception is returned as an error. Explicit
cancellation prevents execution only while the operation is still queued;
once execution starts, its result wins. Ignoring the returned request detaches
it instead of cancelling it, preserving fire-and-forget calls. Raw posts have
no deadline. Register a `std::shared_ptr<EWrapper>` with
`session->ibkr().observe(...)`; the RAII `Subscription` removes it.

Raw observers receive every traditional and Protobuf callback exactly as IBKR
delivers it. Internal correlation runs before observers. The normalized layer
consumes only traditional callbacks because IBKR can emit a Protobuf callback
and its converted traditional counterpart for the same message.

The Unix and Windows generated Protobuf trees are separate build profiles.
Their generated symbols overlap and their runtime versions differ, so two
profiles cannot be linked into or loaded by one process.

## Validation boundary

Session and historical-data behavior is tested with the private backend seam
and synthetic callbacks. Live TWS/IB Gateway tests are manual and opt-in; the
historical operation is not represented as end-to-end verified until that
testing is available. Synthetic fixtures are not described as captured IBKR
traffic.
