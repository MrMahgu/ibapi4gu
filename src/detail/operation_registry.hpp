#pragma once

#include <chrono>
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include <ibapi4gu/error.hpp>
#include <ibapi4gu/request.hpp>
#include <ibapi4gu/types.hpp>

namespace ibapi4gu {
namespace detail {

class PendingOperation {
 public:
  PendingOperation(RequestId id,
                   const std::chrono::steady_clock::time_point& deadline,
                   const std::function<void(RequestId)>& cancel_wire)
      : id_(id), deadline_(deadline), cancel_wire_(cancel_wire) {}
  virtual ~PendingOperation() {}

  RequestId id() const { return id_; }
  const std::chrono::steady_clock::time_point& deadline() const {
    return deadline_;
  }

  void cancel_wire() {
    if (cancel_wire_) {
      cancel_wire_(id_);
    }
  }

  virtual void fail(const Error& error) = 0;

 private:
  RequestId id_;
  std::chrono::steady_clock::time_point deadline_;
  std::function<void(RequestId)> cancel_wire_;
};

template <typename T>
class ValueOperation : public PendingOperation {
 public:
  ValueOperation(RequestId id,
                 const std::chrono::steady_clock::time_point& deadline,
                 const std::function<void(RequestId)>& cancel_wire,
                 const std::shared_ptr<RequestState<T> >& state)
      : PendingOperation(id, deadline, cancel_wire), state_(state) {}

  void succeed(T value) {
    state_->complete(Result<T>::success(std::move(value)));
  }

  void fail(const Error& error) override {
    state_->complete(Result<T>::failure(error));
  }

 private:
  std::shared_ptr<RequestState<T> > state_;
};

template <typename Row>
class RowsOperation : public ValueOperation<std::vector<Row> > {
 public:
  RowsOperation(
      RequestId id, const std::chrono::steady_clock::time_point& deadline,
      const std::function<void(RequestId)>& cancel_wire,
      const std::shared_ptr<RequestState<std::vector<Row> > >& state)
      : ValueOperation<std::vector<Row> >(id, deadline, cancel_wire, state) {}

  void append(const Row& row) { rows_.push_back(row); }

  void finish() {
    ValueOperation<std::vector<Row> >::succeed(std::move(rows_));
  }

 private:
  std::vector<Row> rows_;
};

class OperationRegistry {
 public:
  bool add(const std::shared_ptr<PendingOperation>& operation) {
    return operations_.insert(
        std::make_pair(operation->id().value(), operation)).second;
  }

  template <typename Operation>
  std::shared_ptr<Operation> find(RequestId id) const {
    std::map<int, std::shared_ptr<PendingOperation> >::const_iterator iterator =
        operations_.find(id.value());
    if (iterator == operations_.end()) {
      return std::shared_ptr<Operation>();
    }
    return std::dynamic_pointer_cast<Operation>(iterator->second);
  }

  template <typename Operation>
  std::shared_ptr<Operation> take(RequestId id) {
    std::map<int, std::shared_ptr<PendingOperation> >::iterator iterator =
        operations_.find(id.value());
    if (iterator == operations_.end()) {
      return std::shared_ptr<Operation>();
    }
    const std::shared_ptr<Operation> result =
        std::dynamic_pointer_cast<Operation>(iterator->second);
    if (result) {
      operations_.erase(iterator);
    }
    return result;
  }

  bool fail(RequestId id, const Error& error) {
    std::map<int, std::shared_ptr<PendingOperation> >::iterator iterator =
        operations_.find(id.value());
    if (iterator == operations_.end()) {
      return false;
    }
    const std::shared_ptr<PendingOperation> operation = iterator->second;
    operations_.erase(iterator);
    operation->fail(error);
    return true;
  }

  bool cancel(RequestId id, const Error& error) {
    std::map<int, std::shared_ptr<PendingOperation> >::iterator iterator =
        operations_.find(id.value());
    if (iterator == operations_.end()) {
      return false;
    }
    const std::shared_ptr<PendingOperation> operation = iterator->second;
    operations_.erase(iterator);
    try {
      operation->cancel_wire();
    } catch (...) {
    }
    operation->fail(error);
    return true;
  }

  void expire(const std::chrono::steady_clock::time_point& now) {
    std::map<int, std::shared_ptr<PendingOperation> >::iterator iterator =
        operations_.begin();
    while (iterator != operations_.end()) {
      if (iterator->second->deadline() > now) {
        ++iterator;
        continue;
      }
      const std::shared_ptr<PendingOperation> operation = iterator->second;
      operations_.erase(iterator++);
      try {
        operation->cancel_wire();
      } catch (...) {
      }
      Error error(ErrorCode::timeout, "request timed out", 0, 0,
                  operation->id().value());
      error.scope = ErrorScope::request;
      operation->fail(error);
    }
  }

  void fail_all(const Error& error) {
    std::map<int, std::shared_ptr<PendingOperation> > operations;
    operations.swap(operations_);
    for (std::map<int, std::shared_ptr<PendingOperation> >::iterator iterator =
             operations.begin();
         iterator != operations.end(); ++iterator) {
      Error related = error;
      related.related_id = iterator->first;
      iterator->second->fail(related);
    }
  }

  std::size_t size() const { return operations_.size(); }

 private:
  std::map<int, std::shared_ptr<PendingOperation> > operations_;
};

}  // namespace detail
}  // namespace ibapi4gu
