#pragma once

#include <memory>

class QQmlEngine;
class QQuickItem;
class QQuickWindow;

namespace rnqnx::views {

enum class WindowFrame { Client, System };

// Optional platform window chrome. React/Fabric owns only contentItem().
// The engine and window must outlive this object.
class QtDesktopWindow final {
 public:
  QtDesktopWindow(QQmlEngine& engine, QQuickWindow& window,
      WindowFrame frame = WindowFrame::Client);
  ~QtDesktopWindow();
  QtDesktopWindow(const QtDesktopWindow&) = delete;
  QtDesktopWindow& operator=(const QtDesktopWindow&) = delete;

  QQuickItem& contentItem();

 private:
  std::unique_ptr<QQuickItem> root_;
  QQuickItem* content_{nullptr};
};

}  // namespace rnqnx::views
