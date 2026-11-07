#pragma once

#include <future>
#include <memory>
#include <string>

namespace nitroforge::native {
class ForgeSerialQueue;
}

namespace rnqnx::forge {
class ForgeWorker;
class QnxForgeDocument;

/**
 * Native host owner for Forge's existing Rust engines. Construction starts the
 * worker; open() resolves to a ready document or rejects with the engine error.
 * This internal C++ seam does not install Nitro bindings or a JS dispatcher.
 */
class QnxForgeRuntime final {
 public:
  QnxForgeRuntime();
  ~QnxForgeRuntime();
  QnxForgeRuntime(const QnxForgeRuntime&) = delete;
  QnxForgeRuntime& operator=(const QnxForgeRuntime&) = delete;

  std::future<std::shared_ptr<QnxForgeDocument>> open(std::string engineKind);

  /**
   * Idempotent owner-thread teardown. Rejects new work, waits for accepted work,
   * closes all documents, then joins the worker. Do not call from UI hot paths.
   * Documents may outlive this runtime; their subsequent operations reject.
   */
  void shutdown();

 private:
  std::shared_ptr<ForgeWorker> _worker;
  std::shared_ptr<nitroforge::native::ForgeSerialQueue> _openQueue;
};

} // namespace rnqnx::forge
