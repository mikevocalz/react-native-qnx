#pragma once

#include "ForgeRustSession.hpp"

#include <cstdint>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace nitroforge::native {
class ForgeSerialQueue;
}

namespace rnqnx::forge {
class ForgeWorker;
class QnxForgeRuntime;

/**
 * One ready native document returned by QnxForgeRuntime::open(). Arguments and
 * results own their storage. All engine work runs FIFO on the runtime worker.
 * The command envelope is internal and preserves Forge's per-document v1 ABI;
 * it is not a new public JavaScript API. No raw Rust handles escape this type.
 */
class QnxForgeDocument final {
 public:
  ~QnxForgeDocument();
  QnxForgeDocument(const QnxForgeDocument&) = delete;
  QnxForgeDocument& operator=(const QnxForgeDocument&) = delete;

  std::future<nitroforge::native::CommandResult> apply(
      std::string operation, std::optional<std::string> payloadJson = std::nullopt,
      uint32_t commandVersion = 1);
  std::future<std::vector<uint8_t>> snapshot();
  std::future<void> restore(std::vector<uint8_t> bytes);
  // A FIFO barrier. Repeated calls share its completion; later operations fail.
  std::shared_future<void> close();

 private:
  friend class QnxForgeRuntime;
  QnxForgeDocument(std::shared_ptr<nitroforge::native::ForgeRustSession> session,
                   std::shared_ptr<nitroforge::native::ForgeSerialQueue> queue,
                   std::weak_ptr<ForgeWorker> worker);

  std::shared_ptr<nitroforge::native::ForgeRustSession> _session;
  std::shared_ptr<nitroforge::native::ForgeSerialQueue> _queue;
  std::weak_ptr<ForgeWorker> _worker;
  // Linearizes submissions with the close barrier. No engine/JS code runs here.
  std::mutex _submissionMutex;
  std::shared_future<void> _closed;
};

} // namespace rnqnx::forge
