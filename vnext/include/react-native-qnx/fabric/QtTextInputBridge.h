#pragma once

#include <QObject>
#include <QPointer>
#include <QQuickItem>
#include <QString>

#include <cstdint>
#include <functional>
#include <deque>
#include <optional>

namespace rnqnx::fabric {

enum class TextInputEvent { Change, Selection, Focus, Blur, Key, Submit };
struct TextInputObservation {
  QString text;
  QString key;
  int eventCount;
  int selectionStart;
  int selectionEnd;
  std::uint64_t generation;
};

/// One Qt-owner editing session. Parented to the mounted wrapper, with no JS
/// runtime ownership. Native handles never enter application state or traces.
class QtTextInputBridge final : public QObject {
  Q_OBJECT
 public:
  using Emit = std::function<void(TextInputEvent, const TextInputObservation&)>;
  explicit QtTextInputBridge(QQuickItem& wrapper);
  void setEmitter(Emit callback);
  void beginJSUpdate();
  // Explicit finish can invoke observers and throw; never call it from a destructor.
  void endJSUpdate();
  // Exceptional exit balances the scope and discards pending observations.
  void cancelJSUpdate() noexcept;
  bool configure(bool secure, bool multiline);
  bool applyJS(int eventCount, const std::optional<QString>& text,
               int selectionStart = -1, int selectionEnd = -1);
  int eventCount() const { return eventCount_; }
  std::uint64_t generation() const { return generation_; }
  void invalidate() noexcept;

 public Q_SLOTS:
  void textChanged();
  void selectionChanged();
  void keyPressed();
  void submitEditing();
  void focusChanged();
  void updateTabEligibility();

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  TextInputObservation observation() const;
  void emitEvent(TextInputEvent kind);
  void requireOwner() const;
  QPointer<QQuickItem> wrapper_;
  QPointer<QQuickItem> field_;
  Emit emit_;
  const std::uint64_t generation_;
  int eventCount_{0};
  int lastStart_{-1};
  int lastEnd_{-1};
  int lastSelectionCount_{-1};
  std::uint64_t selectionEpoch_{0};
  bool selectionQueued_{false};
  struct PendingFocus {
    TextInputEvent kind;
    TextInputObservation value;
  };
  std::deque<PendingFocus> pendingFocus_;
  unsigned applyingJS_{0};
  bool drainingFocus_{false};
  bool live_{true};
  bool secure_{false};
  bool hiddenUnsupported_{false};
};
} // namespace rnqnx::fabric
