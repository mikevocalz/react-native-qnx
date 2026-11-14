#pragma once

#include <QObject>
#include <QPointer>
#include <QQuickItem>
#include <QQuickWindow>

#include <functional>

namespace rnqnx::fabric {

/// Gives a React Native Modal its keyboard behaviour on a Qt Quick surface.
///
/// The most recently opened modal in a window owns its keyboard focus. Focus
/// outside that modal returns to it, including focus in a covered modal, so
/// Tab and clicks on the covered app cannot take the keyboard. The item that
/// had focus before the modal opened gets it back when the modal leaves the
/// window. The item's `requestClose()` signal (Escape, from its Qt Quick Keys
/// handler) calls `onRequestClose`, which sends onRequestClose to JavaScript.
///
/// The bridge is parented to the modal item and dies with it.
class QtModalBridge final : public QObject {
  Q_OBJECT

 public:
  /// `modal` must declare a `requestClose()` signal and the object name
  /// returned by `modalObjectName()`.
  QtModalBridge(QQuickItem& modal, std::function<void()> onRequestClose);
  ~QtModalBridge() override;

  static QString modalObjectName();

 private Q_SLOTS:
  void windowChanged(QQuickWindow* window);
  void activeFocusChanged();
  void requestClose();

 private:
  void enter(QQuickWindow& window);
  void leave();
  static QtModalBridge* topModal(QQuickWindow* window);
  bool contains(const QQuickItem* item) const;

  QQuickItem& modal_;
  std::function<void()> onRequestClose_;
  QPointer<QQuickWindow> window_;
  QPointer<QQuickItem> previousFocus_;
  bool reclaiming_{false};
};

}  // namespace rnqnx::fabric
