#include "react-native-qnx/fabric/QtTextInputBridge.h"

#include <QEvent>
#include <QDebug>
#include <QInputMethodEvent>
#include <QInputMethodQueryEvent>
#include <QThread>

#include <atomic>
#include <limits>
#include <stdexcept>
#include <utility>

namespace rnqnx::fabric {
namespace {
std::atomic<std::uint64_t> nextGeneration{1};
}
QtTextInputBridge::QtTextInputBridge(QQuickItem& wrapper)
    : QObject(&wrapper), wrapper_(&wrapper),
      generation_(nextGeneration.fetch_add(1, std::memory_order_relaxed)) {
  wrapper.setProperty("contractEnabled", true);
  QObject::connect(&wrapper, SIGNAL(textChanged()), this, SLOT(textChanged()));
  QObject::connect(&wrapper, SIGNAL(selectionChanged()), this, SLOT(selectionChanged()));
  QObject::connect(&wrapper, SIGNAL(keyPressed()), this, SLOT(keyPressed()));
  QObject::connect(&wrapper, SIGNAL(submitEditing()), this, SLOT(submitEditing()));
  QObject::connect(&wrapper, &QQuickItem::activeFocusChanged, this, &QtTextInputBridge::focusChanged);
  for (const auto* name : {"qnx-text-input", "qnx-text-edit"}) {
    if (auto* child = wrapper.findChild<QQuickItem*>(name)) child->installEventFilter(this);
  }
  QObject::connect(&wrapper, SIGNAL(readOnlyChanged()), this, SLOT(updateTabEligibility()));
  wrapper.installEventFilter(this);
  field_ = wrapper.findChild<QQuickItem*>("qnx-text-input");
  updateTabEligibility();
}
void QtTextInputBridge::requireOwner() const {
  if (QThread::currentThread() != thread()) throw std::runtime_error("TextInput requires its Qt owner");
}
void QtTextInputBridge::setEmitter(Emit callback) {
  requireOwner();
  emit_ = std::move(callback);
}
void QtTextInputBridge::beginJSUpdate() { requireOwner(); ++applyingJS_; }
void QtTextInputBridge::endJSUpdate() {
  requireOwner();
  if (!applyingJS_) throw std::logic_error("Unbalanced TextInput JS update");
  --applyingJS_;
  if (applyingJS_ || drainingFocus_) return;
  drainingFocus_ = true;
  const QPointer<QtTextInputBridge> self(this);
  struct ResetDrain {
    QPointer<QtTextInputBridge> bridge;
    ~ResetDrain() { if (bridge) bridge->drainingFocus_ = false; }
  } reset{this};
  try {
    while (self && live_ && !applyingJS_ && !pendingFocus_.empty()) {
      // Pop before invoking user code. Deletion cannot invalidate this payload,
      // and nested updates append after every already accepted observation.
      auto event = std::move(pendingFocus_.front());
      pendingFocus_.pop_front();
      const auto callback = emit_;
      if (callback) callback(event.kind, event.value);
    }
  } catch (...) {
    if (self) pendingFocus_.clear();
    throw;
  }
}
void QtTextInputBridge::cancelJSUpdate() noexcept {
  if (applyingJS_) --applyingJS_;
  pendingFocus_.clear();
}
void QtTextInputBridge::invalidate() noexcept {
  live_ = false;
  ++selectionEpoch_;
  selectionQueued_ = false;
  emit_ = {};
  pendingFocus_.clear();
  updateTabEligibility();
}
void QtTextInputBridge::updateTabEligibility() {
  const QPointer<QQuickItem> wrapper = wrapper_;
  if (!wrapper) return;
  const bool eligible = live_ && !wrapper->property("readOnly").toBool();
  // Qt refuses to remove an active item from the tab chain. Release this
  // wrapper's focus first when editing is disabled or the session is invalid.
  if (!eligible && wrapper->hasActiveFocus()) wrapper->setFocus(false);
  if (wrapper) wrapper->setActiveFocusOnTab(eligible);
}
bool QtTextInputBridge::configure(bool secure, bool multiline) {
  requireOwner();
  if (!live_ || !wrapper_) return false;
  const QPointer<QtTextInputBridge> self(this);
  const QPointer<QQuickItem> wrapper(wrapper_);
  secure_ = secure;
  if (secure && multiline) {
    // Fail before applying incoming text. Previously visible nonsecure content
    // must not remain visible when this unsupported secure mode is requested.
    wrapper->setProperty("readOnly", true);
    if (!self || !wrapper) return false;
    wrapper->setVisible(false);
    if (!self || !wrapper) return false;
    hiddenUnsupported_ = true;
    return false;
  }
  field_ = wrapper_->findChild<QQuickItem*>(multiline ? "qnx-text-edit" : "qnx-text-input");
  wrapper->setProperty("echoMode", secure ? 2 : 0);
  if (!self || !wrapper) return false;
  wrapper->setProperty("multiline", multiline);
  if (!self || !wrapper) return false;
  if (hiddenUnsupported_) {
    wrapper->setVisible(true);
    if (!self || !wrapper) return false;
    hiddenUnsupported_ = false;
  }
  return !field_.isNull();
}
TextInputObservation QtTextInputBridge::observation() const {
  return {field_ ? field_->property("text").toString() : QString(),
          wrapper_ ? wrapper_->property("lastKeyText").toString() : QString(),
          eventCount_, field_ ? field_->property("selectionStart").toInt() : 0,
          field_ ? field_->property("selectionEnd").toInt() : 0, generation_};
}
void QtTextInputBridge::emitEvent(TextInputEvent kind) {
  const bool focusEvent = kind == TextInputEvent::Focus || kind == TextInputEvent::Blur;
  if (!live_ || !field_ || !emit_) return;
  if (applyingJS_ && !focusEvent) return;
  const auto value = observation();
  if (focusEvent && (applyingJS_ || drainingFocus_)) {
    // Qt still owns its focus-notification/property-write stack here. Deliver
    // only after the outer JS update finishes, so observers can safely delete
    // the wrapper. Preserve the actual transition's count/text snapshot.
    pendingFocus_.push_back({kind, value});
    return;
  }
  const auto callback = emit_;
  callback(kind, value); // No access after user code, which may delete this.
}
void QtTextInputBridge::textChanged() {
  if (!live_ || applyingJS_ || !field_) return;
  if (eventCount_ == std::numeric_limits<int>::max()) {
    qWarning("RNQNX_TEXT_INPUT_EVENT_COUNT_EXHAUSTED");
    invalidate(); // A wrapped ordering token cannot safely acknowledge edits.
    return;
  }
  ++eventCount_;
  emitEvent(TextInputEvent::Change);
}
void QtTextInputBridge::selectionChanged() {
  if (!live_ || applyingJS_ || !field_ || selectionQueued_) return;
  // Qt can announce a cursor endpoint before its committed text notification.
  // Coalesce on this QObject's existing owner event loop so RN observes the
  // committed edit token first. Destruction cancels the receiver-bound call.
  selectionQueued_ = true;
  const auto epoch = selectionEpoch_;
  QMetaObject::invokeMethod(this, [this, epoch] {
    if (!live_ || epoch != selectionEpoch_) return;
    selectionQueued_ = false;
    if (!field_) return;
    const auto value = observation();
    if (value.selectionStart == lastStart_ && value.selectionEnd == lastEnd_ &&
        value.eventCount == lastSelectionCount_) return;
    lastStart_ = value.selectionStart;
    lastEnd_ = value.selectionEnd;
    lastSelectionCount_ = value.eventCount;
    emitEvent(TextInputEvent::Selection);
  }, Qt::QueuedConnection);
}

void QtTextInputBridge::keyPressed() { emitEvent(TextInputEvent::Key); }
void QtTextInputBridge::submitEditing() { emitEvent(TextInputEvent::Submit); }
void QtTextInputBridge::focusChanged() {
  if (!wrapper_) return;
  emitEvent(wrapper_->hasActiveFocus() ? TextInputEvent::Focus : TextInputEvent::Blur);
}
bool QtTextInputBridge::applyJS(int eventCount, const std::optional<QString>& text,
                               int selectionStart, int selectionEnd) {
  requireOwner();
  if (!live_ || !field_ || eventCount != eventCount_) return false;
  beginJSUpdate();
  const QPointer<QtTextInputBridge> self(this);
  struct FinishUpdate {
    QPointer<QtTextInputBridge> bridge;
    bool active{true};
    ~FinishUpdate() { if (active && bridge) bridge->cancelJSUpdate(); }
    void finish() {
      active = false;
      if (bridge) bridge->endJSUpdate();
    }
  } update{this};
  const auto applyProperties = [&]() -> bool {
  bool changed = false;
  if (text && field_->property("text").toString() != *text) {
    field_->setProperty("text", *text);
    if (!self || !field_) return false;
    changed = true;
  }
  const auto size = field_->property("text").toString().size();
  // Match RN 0.88.1: apply matching-token text first, then skip a selection
  // whose positions cannot be resolved. The paired -1 sentinel also skips it.
  if (selectionStart >= 0 && selectionEnd >= selectionStart && selectionEnd <= size) {
    changed = true;
    if (!QMetaObject::invokeMethod(field_, "select", Q_ARG(int, selectionStart), Q_ARG(int, selectionEnd))) {
      throw std::runtime_error("TextInput selection method unavailable");
    }
  }
  if (!self) return false;
  if (changed && field_) {
    ++selectionEpoch_; // A real JS replacement supersedes queued native selection.
    selectionQueued_ = false;
    lastSelectionCount_ = eventCount_;
    lastStart_ = field_->property("selectionStart").toInt();
    lastEnd_ = field_->property("selectionEnd").toInt();
  }
  return true;
  };
  const bool result = applyProperties();
  update.finish(); // Direct commands also finish outside every Qt setter.
  return self && live_ && result;
}
bool QtTextInputBridge::eventFilter(QObject* watched, QEvent* event) {
  // Invalidated fields can still exist briefly while Qt detaches the item.
  // Keep their query/commit route closed until QObject destruction finishes.
  if (live_ && !secure_) return QObject::eventFilter(watched, event);
  if (event->type() == QEvent::InputMethodQuery) {
    auto* query = static_cast<QInputMethodQueryEvent*>(event);
    // Secure IME is explicitly unavailable. Answer no sensitive text query,
    // including queries directed at the inactive multiline child.
    query->setValue(Qt::ImEnabled, false);
    query->setValue(Qt::ImHints, static_cast<int>(Qt::ImhSensitiveData | Qt::ImhNoPredictiveText));
    for (const auto type : {Qt::ImSurroundingText, Qt::ImCurrentSelection,
                            Qt::ImTextBeforeCursor, Qt::ImTextAfterCursor}) {
      query->setValue(type, QString());
    }
    query->accept();
    return true;
  }
  if (event->type() == QEvent::InputMethod) {
    event->ignore();
    return true;
  }
  return QObject::eventFilter(watched, event);
}
} // namespace rnqnx::fabric
