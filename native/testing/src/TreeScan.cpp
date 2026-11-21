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
// `supported` is false for a type with no visibility.
bool isVisibleObject(QObject* object, bool& supported) {
  supported = true;
  if (const auto* item = qobject_cast<QQuickItem*>(object)) return item->isVisible();
  if (const auto* window = qobject_cast<QWindow*>(object)) return window->isVisible();
  supported = false;
  return false;
}

class Scan {
 public:
  explicit Scan(const NodeQuery& query) : query_(query) {}

  void visit(QObject* object) {
    if (outcome_.node != nullptr || object == nullptr || visited_.contains(object)) return;
    visited_.insert(object);
    if (object->objectName() == query_.objectName) {
      bool supported = true;
      if (!query_.requireVisible || isVisibleObject(object, supported)) {
        outcome_.node = object;
        return;
      }
      // A match without visibility is not a match; a later sibling may still
      // satisfy the query. The caller reports it if nothing does.
      if (supported) outcome_.nameMatchedHidden = true;
      else if (outcome_.unsupportedClass.isEmpty())
        outcome_.unsupportedClass = QString::fromLatin1(object->metaObject()->className());
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
  QSet<const QObject*> visited_;
  ScanOutcome outcome_;
};

}  // namespace

ScanOutcome scanTree(QObject* root, const NodeQuery& query) {
  Scan scan(query);
  scan.visit(root);
  return scan.outcome();
}

void throwIfUnsupported(const ScanOutcome& outcome, const char* call) {
  if (outcome.node == nullptr && !outcome.unsupportedClass.isEmpty())
    throw std::logic_error(std::string(call) + ": requireVisible needs a QQuickItem or QWindow, '" +
                           outcome.unsupportedClass.toStdString() + "' has no visibility");
}

void checkQuery(QObject* root, const NodeQuery& query, const char* call) {
  if (root == nullptr) throw std::invalid_argument(std::string(call) + ": root is null");
  if (query.objectName.isEmpty())
    throw std::invalid_argument(std::string(call) + ": objectName is empty");
  if (root->thread() != QThread::currentThread())
    throw std::logic_error(std::string(call) + ": root lives in another thread");
}

}  // namespace rnqnx::testing::detail
