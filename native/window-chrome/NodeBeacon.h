#pragma once

// Prints `RNQNX_NODE name=<objectName> visible=1` once per named node, after
// the node is visible and a frame has been presented. scripts/lib/wait-log.sh
// (rnqnx_wait_node) waits for these lines, so a device smoke path waits on the
// app's tree and not on the clock. See docs/evidence/s3-node-beacon-2026-11-21.md.
//
// Lives on the GUI thread. The frame signal can come from the render thread;
// the connection is queued onto the thread of the beacon.

#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>

class QQuickItem;
class QQuickWindow;

namespace rnqnx::window {

class NodeBeacon final : public QObject {
  Q_OBJECT

 public:
  using Sink = std::function<void(const QString& line)>;

  // Watches `names` under `root` (the root can match). `root` and `window`
  // must outlive the beacon. The default sink writes the line to stdout and
  // flushes, because stdout is a file on the device.
  NodeBeacon(QQuickWindow& window, QQuickItem& root, QStringList names, Sink sink = printToStdout);
  ~NodeBeacon() override;

  static void printToStdout(const QString& line);

  // Names not reported yet.
  QStringList pending() const { return pending_; }

 private:
  void scan();

  QQuickItem& root_;
  QStringList pending_;
  Sink sink_;
};

}  // namespace rnqnx::window
