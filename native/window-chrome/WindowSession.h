#pragma once

#include "WindowChrome.h"
#include <QObject>
#include <QVariantMap>
#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

namespace rnqnx::window {
class WindowSession;
// The only cross-thread data. Qt objects are accessed only by the GUI owner.
// Admission and receiver destruction share the mutex; no callback runs under it.
struct WindowSessionState {
  using Change = std::function<void(std::uint64_t, const QVariantMap&, bool)>;
  struct Pending { std::function<void(WindowSession&)> run; std::function<void()> reject; };
  std::mutex mutex;
  WindowSession* owner{nullptr};
  bool closeVetoAvailable{true};
  QVariantMap snapshot;
  std::uint64_t sequence{0};
  std::uint64_t nextId{1};
  std::map<std::uint64_t, Pending> pending;
  std::map<std::uint64_t, Change> listeners;
  std::vector<std::weak_ptr<std::atomic<bool>>> guards;
  bool submit(std::function<void(WindowSession&)> run, std::function<void()> reject);
  std::uint64_t listen(Change change);
  void unlisten(std::uint64_t id);
  std::shared_ptr<std::atomic<bool>> guard();
  bool blocked();
  void shutdown();
};

// GUI-owned single-window controller. Neither sessions nor handles create a window.
class WindowSession final : public QObject {
 public:
  WindowSession(QQuickWindow& window, WindowChrome* chrome, WindowCapabilities capabilities);
  ~WindowSession() override;
  std::shared_ptr<WindowSessionState> state() const { return state_; }
  void setTitle(const QString& title);
  void setSize(double width, double height);
  void setPosition(double x, double y);
  void setControlsLayout(const QString& layout);
  void minimize();
  void restore();
  void setVisible(bool visible);
  void requestFocus();
  bool close();
  bool cancelCloseRequest(const QString& requestId);
  void shutdown();
 protected:
  bool eventFilter(QObject* object, QEvent* event) override;
 private:
  void publish();
  void requireWindow() const;
  QPointer<QQuickWindow> window_;
  QPointer<WindowChrome> chrome_;
  WindowCapabilities capabilities_;
  std::shared_ptr<WindowSessionState> state_;
  bool canResize_{false};
  bool canPosition_{false};
  bool closeRequested_{false};
  QString closeRequestEpoch_;
  QString closeRequestId_;
  std::uint64_t closeRequestSequence_{0};
  bool minimized_{false};
};
} // namespace rnqnx::window
