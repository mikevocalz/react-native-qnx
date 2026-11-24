#pragma once

// The close path of the Nitro windows API (docs/nitro/API_CONTRACTS.md,
// "Closing, guards and who can be asked"). It hooks QEvent::Close on the
// QQuickWindow, so the close control, close() on the window and the API's own
// close() all pass one gate. The gate holds ids only: JavaScript callbacks
// live on the JavaScript side, which sequences the guards and reports back
// through settle(). Every method runs on the GUI thread.

#include "WindowCloseRequest.h"

#include <QObject>
#include <QPointer>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

class QEvent;
class QQuickWindow;

namespace rnqnx::window {

class WindowCloseGate final : public QObject {
 public:
  using RequestHandler = std::function<void(const CloseRequestInfo&)>;
  // Called once with true when the close was accepted (before the native close
  // is posted) and false when a guard cancelled it.
  using Waiter = std::function<void(bool accepted)>;

  explicit WindowCloseGate(QQuickWindow& window, QObject* parent = nullptr);
  ~WindowCloseGate() override;

  // Bumped when the JavaScript runtime that registered guards goes away. A
  // result or a post that carries an older value is dropped. Shared so the
  // JavaScript side can bump it from its teardown hook without a thread hop.
  std::shared_ptr<std::atomic<std::uint64_t>> epochCounter() const { return epoch_; }

  // Where "guards must be asked" goes. Without one, every close proceeds.
  void setRequestHandler(RequestHandler handler);

  void addGuard(std::uint64_t id);
  void removeGuard(std::uint64_t id);
  std::size_t guardCount() const { return guards_.size(); }
  bool pending() const { return pending_.has_value(); }

  // The next QEvent::Close comes from `reason` (the chrome's close control
  // announces itself as Control). Consumed by one close event.
  void noteNextClose(CloseReason reason, bool cancellable, int deadlineMs);

  // A close nobody announced through noteNextClose() or requestClose() came
  // from the platform (the window manager's close message, a native frame
  // button) and has reason `system`.
  // `cancellable` says whether the source lets a guard cancel it; under
  // rnqnx-winmgr it does not (ADR-0016 item 11: SIGTERM follows the grace
  // period). `deadlineMs` is -1 when the source states none. A system close
  // that arrives while the window is hidden is never cancellable (N4): a
  // minimized window cannot show the guard's question.
  void setSystemClose(bool cancellable, int deadlineMs);
  // True while the window can be seen. Defaults to the QWindow's own state;
  // the host adds what Qt cannot see (rnqnx-winmgr hides a window it minimizes).
  void setVisibleProbe(std::function<bool()> visible);

  // The API's close(). `force` skips the guards and overrides a pending
  // close. Without a registered guard it closes at once.
  void requestClose(bool force, Waiter waiter);

  // A guard sequence ended. Dropped when the epoch is old, nothing is pending
  // or the id is not the pending request's.
  void settle(std::uint64_t requestId, std::uint64_t epoch, bool allow);

  // Teardown on the GUI side: forget every guard id; a pending close proceeds
  // unguarded. The owner has already bumped the epoch.
  void runtimeGone();
  // bumpEpoch() then runtimeGone(), for callers on the GUI thread.
  void resetRuntime();

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  struct Next {
    CloseReason reason;
    bool cancellable;
    int deadlineMs;
  };
  void syncEpoch();
  void begin(CloseReason reason, bool cancellable, int deadlineMs);
  bool pendingIsStale() const;
  void finishAccepted(bool closeWindow);
  void finishCancelled();

  QPointer<QQuickWindow> window_;
  std::shared_ptr<std::atomic<std::uint64_t>> epoch_{std::make_shared<std::atomic<std::uint64_t>>(1)};
  RequestHandler handler_;
  bool systemCancellable_{true};
  int systemDeadlineMs_{-1};
  std::function<bool()> visibleProbe_;
  std::vector<std::uint64_t> guards_;
  std::optional<CloseRequestInfo> pending_;
  std::vector<Waiter> waiters_;
  std::optional<Next> next_;
  std::uint64_t nextRequestId_{0};
  std::uint64_t seenEpoch_{1};
  bool bypass_{false};
  // The application is quitting (QCoreApplication::quit()): close events pass.
  bool quitting_{false};
};

}  // namespace rnqnx::window
