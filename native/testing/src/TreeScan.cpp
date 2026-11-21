#include "TreeScan.h"

#include <QQuickItem>
#include <QQuickWindow>
#include <QSet>
#include <QThread>
#include <QWindow>

#include <stdexcept>
#include <string>

namespace rnqnx::testing::detail {
namespace {

// Visibility by dynamic type, so the scan reads no user-defined getter.
bool isVisibleObject(QObject* object, const char* call) {
  if (const auto* item = qobject_cast<QQuickItem*>(object)) return item->isVisible();
  if (const auto* window = qobject_cast<QWindow*>(object)) return window->isVisible();
  throw std::logic_error(std::string(call) + ": requireVisible needs a QQuickItem or QWindow, '" +
                         object->metaObject()->className() + "' has no visibility");
}

class Scan {
 public:
  Scan(const NodeQuery& query, const char* call) : query_(query), call_(call) {}

  void visit(QObject* object) {
    if (outcome_.node != nullptr || object == nullptr || visited_.contains(object)) return;
    visited_.insert(object);
    if (object->objectName() == query_.objectName) {
      if (!query_.requireVisible || isVisibleObject(object, call_)) {
        outcome_.node = object;
        return;
      }
      outcome_.nameMatchedHidden = true;
    }
    // Copy the lists: visiting reads only, but a copy keeps the walk valid if a
    // getter ever changes the tree.
    const QObjectList children = object->children();
    for (QObject* child : children) visit(child);
    if (const auto* item = qobject_cast<QQuickItem*>(object)) {
      const QList<QQuickItem*> items = item->childItems();
      for (QQuickItem* child : items) visit(child);
    }
    if (const auto* window = qobject_cast<QQuickWindow*>(object)) visit(window->contentItem());
  }

  ScanOutcome outcome() const { return outcome_; }

 private:
  const NodeQuery& query_;
  const char* call_;
  QSet<const QObject*> visited_;
  ScanOutcome outcome_;
};

}  // namespace

ScanOutcome scanTree(QObject* root, const NodeQuery& query) {
  Scan scan(query, "scanTree");
  scan.visit(root);
  return scan.outcome();
}

void checkQuery(QObject* root, const NodeQuery& query, const char* call) {
  if (root == nullptr) throw std::invalid_argument(std::string(call) + ": root is null");
  if (query.objectName.isEmpty())
    throw std::invalid_argument(std::string(call) + ": objectName is empty");
  if (root->thread() != QThread::currentThread())
    throw std::logic_error(std::string(call) + ": root lives in another thread");
}

}  // namespace rnqnx::testing::detail
