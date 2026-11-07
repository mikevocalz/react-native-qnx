#include "QnxForgeDocument.hpp"

#include "ForgeFuture.hpp"

#include <string_view>
#include <utility>

namespace rnqnx::forge {

QnxForgeDocument::QnxForgeDocument(
    std::shared_ptr<nitroforge::native::ForgeRustSession> session,
    std::shared_ptr<nitroforge::native::ForgeSerialQueue> queue,
    std::weak_ptr<ForgeWorker> worker)
    : _session(std::move(session)), _queue(std::move(queue)), _worker(std::move(worker)) {}

QnxForgeDocument::~QnxForgeDocument() {
  if (!_closed.valid()) {
    try {
      // Dropped futures do not cancel the close. The worker also retains live
      // sessions, so rejected admission or allocation failure falls back to
      // shutdown cleanup without destroying a Rust document on the caller.
      detail::dispatch(_worker.lock(), _queue, [session = _session] { session->close(); });
    } catch (...) {
      // Destruction cannot report an allocation failure. Worker shutdown still
      // owns and closes this session; explicit close() reports errors normally.
    }
  }
}

std::future<nitroforge::native::CommandResult> QnxForgeDocument::apply(
    std::string operation, std::optional<std::string> payloadJson, uint32_t commandVersion) {
  std::lock_guard lock(_submissionMutex);
  if (_closed.valid()) {
    return detail::closedDocument<nitroforge::native::CommandResult>();
  }
  return detail::dispatch(_worker.lock(), _queue,
      [session = _session, operation = std::move(operation),
       payload = std::move(payloadJson), commandVersion] {
        return session->apply(operation,
            payload ? std::optional<std::string_view>(*payload) : std::nullopt,
            commandVersion);
      });
}

std::future<std::vector<uint8_t>> QnxForgeDocument::snapshot() {
  std::lock_guard lock(_submissionMutex);
  if (_closed.valid()) {
    return detail::closedDocument<std::vector<uint8_t>>();
  }
  return detail::dispatch(_worker.lock(), _queue,
      [session = _session] { return session->snapshot(); });
}

std::future<void> QnxForgeDocument::restore(std::vector<uint8_t> bytes) {
  std::lock_guard lock(_submissionMutex);
  if (_closed.valid()) {
    return detail::closedDocument<void>();
  }
  return detail::dispatch(_worker.lock(), _queue,
      [session = _session, bytes = std::move(bytes)] { session->restore(bytes); });
}

std::shared_future<void> QnxForgeDocument::close() {
  std::lock_guard lock(_submissionMutex);
  if (!_closed.valid()) {
    _closed = detail::dispatch(_worker.lock(), _queue,
        [session = _session] { session->close(); }).share();
  }
  return _closed;
}

} // namespace rnqnx::forge
