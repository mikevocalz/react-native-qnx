#include "NodeBeacon.h"

#include <QQuickItem>
#include <QQuickWindow>

#include <cstdio>

namespace rnqnx::window {
namespace {

// First visible item named `name` under `item`, the item itself included.
// QQuickItem::isVisible() already counts a hidden ancestor.
bool hasVisibleNode(const QQuickItem* item, const QString& name) {
  if (item->objectName() == name && item->isVisible()) return true;
  const QList<QQuickItem*> children = item->childItems();
  for (const QQuickItem* child : children)
    if (hasVisibleNode(child, name)) return true;
  return false;
}

}  // namespace

NodeBeacon::NodeBeacon(QQuickWindow& window, QQuickItem& root, QStringList names, Sink sink)
    : root_(root), pending_(std::move(names)), sink_(std::move(sink)) {
  // frameSwapped comes from the render thread with the threaded render loop;
  // the context object makes the connection queued onto this thread.
  connect(&window, &QQuickWindow::frameSwapped, this, [this]() { scan(); }, Qt::QueuedConnection);
}

NodeBeacon::~NodeBeacon() = default;

void NodeBeacon::printToStdout(const QString& line) {
  std::printf("%s\n", line.toUtf8().constData());
  std::fflush(stdout);
}

void NodeBeacon::scan() {
  for (qsizetype i = 0; i < pending_.size();) {
    if (hasVisibleNode(&root_, pending_.at(i))) {
      const QString name = pending_.takeAt(i);
      sink_(QStringLiteral("RNQNX_NODE name=%1 visible=1").arg(name));
    } else {
      ++i;
    }
  }
}

}  // namespace rnqnx::window
