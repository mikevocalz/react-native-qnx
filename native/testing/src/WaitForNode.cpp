#include <rnqnx/testing/NodeQuery.h>

#include "TreeScan.h"

#include <QAbstractEventDispatcher>
#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>

#include <algorithm>
#include <climits>
#include <stdexcept>

namespace rnqnx::testing {
namespace {

// One wait per thread. The flag is cleared on every exit path, including a throw.
class ActiveWait {
 public:
  ActiveWait() {
    if (active()) throw std::logic_error("waitForNode: nested waitForNode on this thread");
    active() = true;
  }
  ~ActiveWait() { active() = false; }
  ActiveWait(const ActiveWait&) = delete;
  ActiveWait& operator=(const ActiveWait&) = delete;

 private:
  static bool& active() {
    thread_local bool flag = false;
    return flag;
  }
};

QString describeRoot(const QObject* root) {
  QString text = QString::fromLatin1(root->metaObject()->className());
  if (!root->objectName().isEmpty()) text += QStringLiteral(" '%1'").arg(root->objectName());
  return text;
}

}  // namespace

NodeWaitResult waitForNode(QObject* root, const NodeQuery& query,
                           std::chrono::milliseconds timeout) {
  detail::checkQuery(root, query, "waitForNode");
  if (timeout.count() <= 0) throw std::invalid_argument("waitForNode: timeout must be positive");
  if (QCoreApplication::instance() == nullptr)
    throw std::logic_error("waitForNode: no QCoreApplication");
  ActiveWait activeWait;

  using Clock = std::chrono::steady_clock;
  const auto start = Clock::now();
  const QString rootText = describeRoot(root);
  const QPointer<QObject> guard(root);
  const auto elapsed = [&] {
    return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start);
  };
  const auto finished = [&](NodeWaitStatus status, QObject* node, const QString& error) {
    NodeWaitResult result;
    result.status = status;
    result.node = node;
    result.elapsed = elapsed();
    result.error = error;
    return result;
  };

  detail::ScanOutcome first = detail::scanTree(root, query);
  if (first.node != nullptr) return finished(NodeWaitStatus::Found, first.node, QString());
  detail::throwIfUnsupported(first, "waitForNode");

  bool decided = false;
  NodeWaitStatus status = NodeWaitStatus::AppQuit;  // stays if the loop ends without a verdict
  QObject* hit = nullptr;
  bool hiddenMatch = first.nameMatchedHidden;
  QString unsupported = first.unsupportedClass;

  QEventLoop loop;
  QTimer deadline;
  deadline.setSingleShot(true);
  deadline.setTimerType(Qt::PreciseTimer);

  const auto conclude = [&](NodeWaitStatus verdict, QObject* node) {
    decided = true;
    status = verdict;
    hit = node;
    loop.quit();
  };
  const auto rescan = [&] {
    if (decided) return;
    if (guard.isNull()) return conclude(NodeWaitStatus::RootDestroyed, nullptr);
    const detail::ScanOutcome outcome = detail::scanTree(root, query);
    hiddenMatch = hiddenMatch || outcome.nameMatchedHidden;
    if (unsupported.isEmpty()) unsupported = outcome.unsupportedClass;
    if (outcome.node != nullptr) conclude(NodeWaitStatus::Found, outcome.node);
  };

  QAbstractEventDispatcher* dispatcher = QAbstractEventDispatcher::instance();
  if (dispatcher == nullptr) throw std::logic_error("waitForNode: no event dispatcher");
  QObject::connect(dispatcher, &QAbstractEventDispatcher::aboutToBlock, &loop, rescan);
  QObject::connect(root, &QObject::destroyed, &loop,
                   [&] { conclude(NodeWaitStatus::RootDestroyed, nullptr); });
  QObject::connect(&deadline, &QTimer::timeout, &loop, [&] {
    rescan();  // a node that arrived in the deadline's own turn still counts
    if (!decided) conclude(hiddenMatch ? NodeWaitStatus::NotVisible : NodeWaitStatus::TimedOut, nullptr);
  });
  const auto budget = std::min<std::chrono::milliseconds::rep>(timeout.count(), INT_MAX);
  deadline.start(static_cast<int>(budget));
  loop.exec();

  // Thrown here, after the loop, never from a dispatcher slot.
  if (status == NodeWaitStatus::TimedOut || status == NodeWaitStatus::NotVisible) {
    detail::ScanOutcome late;
    late.unsupportedClass = unsupported;
    detail::throwIfUnsupported(late, "waitForNode");
  }

  const QString name = query.objectName;
  switch (status) {
    case NodeWaitStatus::Found:
      return finished(status, hit, QString());
    case NodeWaitStatus::TimedOut:
      return finished(status, nullptr,
                      QStringLiteral("timed out after %1 ms waiting for node '%2' under %3")
                          .arg(timeout.count())
                          .arg(name, rootText));
    case NodeWaitStatus::NotVisible:
      return finished(status, nullptr,
                      QStringLiteral("node '%1' under %2 exists but was not visible after %3 ms")
                          .arg(name, rootText)
                          .arg(timeout.count()));
    case NodeWaitStatus::RootDestroyed:
      return finished(status, nullptr,
                      QStringLiteral("root %1 was destroyed while waiting for node '%2'")
                          .arg(rootText, name));
    case NodeWaitStatus::AppQuit:
      return finished(status, nullptr,
                      QStringLiteral("the application quit while waiting for node '%1' under %2")
                          .arg(name, rootText));
  }
  return finished(status, nullptr, QString());
}

}  // namespace rnqnx::testing
