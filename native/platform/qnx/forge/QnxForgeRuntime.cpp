#include "QnxForgeRuntime.hpp"

#include "ForgeFuture.hpp"
#include "ForgeRustSession.hpp"
#include "QnxForgeDocument.hpp"

#include <utility>

namespace rnqnx::forge {

QnxForgeRuntime::QnxForgeRuntime()
    : _worker(std::make_shared<ForgeWorker>()), _openQueue(_worker->createQueue()) {}

QnxForgeRuntime::~QnxForgeRuntime() {
  shutdown();
}

std::future<std::shared_ptr<QnxForgeDocument>> QnxForgeRuntime::open(std::string engineKind) {
  return detail::dispatch(_worker, _openQueue,
      [weak = std::weak_ptr<ForgeWorker>(_worker), engineKind = std::move(engineKind)] {
        const auto worker = weak.lock();
        if (!worker) {
          throw std::runtime_error("QNX Forge runtime is shut down");
        }
        auto session = nitroforge::native::ForgeRustSession::open(engineKind);
        try {
          worker->trackSession(session);
          return std::shared_ptr<QnxForgeDocument>(
              new QnxForgeDocument(session, worker->createQueue(), weak));
        } catch (...) {
          session->close();
          throw;
        }
      });
}

void QnxForgeRuntime::shutdown() {
  _worker->shutdown();
}

} // namespace rnqnx::forge
