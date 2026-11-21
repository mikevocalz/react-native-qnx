// Test-support tree query. Linked into host test executables only; see the
// "Native tree query for tests" section of docs/nitro/API_CONTRACTS.md.
#pragma once

#include <QObject>
#include <QPointer>
#include <QString>

#include <chrono>

namespace rnqnx::testing {

struct NodeQuery {
  QString objectName;           // exact match, non-empty
  bool requireVisible = false;  // QQuickItem::isVisible() / QWindow::isVisible()
};

enum class NodeWaitStatus { Found, TimedOut, NotVisible, RootDestroyed, AppQuit };

struct NodeWaitResult {
  NodeWaitStatus status = NodeWaitStatus::TimedOut;
  QPointer<QObject> node;                // set when found; may be null again when read
  std::chrono::milliseconds elapsed{0};  // steady clock
  QString error;                         // empty when found

  bool found() const { return status == NodeWaitStatus::Found; }
};

// One synchronous scan. `root` must live in the calling thread. Returns nullptr
// when no object satisfies the query. Throws std::invalid_argument for a null
// root or an empty name, std::logic_error for a root in another thread or for
// requireVisible on a type with no visibility.
QObject* findNode(QObject* root, const NodeQuery& query);

// Scans now, then pumps the calling thread's event loop until the node exists
// or `timeout` passes. Same preconditions as findNode, plus a positive timeout,
// a QCoreApplication, and no other waitForNode active on this thread.
NodeWaitResult waitForNode(QObject* root, const NodeQuery& query,
                           std::chrono::milliseconds timeout);

}  // namespace rnqnx::testing
