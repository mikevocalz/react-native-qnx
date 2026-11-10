#pragma once

#include "react-native-qnx/fabric/QtQuickMountingLayer.h"
#include <react/renderer/components/view/TouchEventEmitter.h>
#include <react/renderer/components/view/primitives.h>
#include <QHash>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QQuickItem>
#include <memory>
#include <vector>

namespace facebook::react { struct ShadowView; }
QT_BEGIN_NAMESPACE
class QKeyEvent;
class QMouseEvent;
QT_END_NAMESPACE

namespace rnqnx::fabric {

// Owned by QtTouchDispatcher on the Qt thread. Delivers desktop input through
// the existing Fabric emitter; it owns no runtime and performs no JS calls.
class QtDesktopInput final : public QObject {
 public:
  QtDesktopInput(QtQuickMountingLayer& layer, QQuickItem& root);
  ~QtDesktopInput() override;
  void setView(const facebook::react::ShadowView& view);
  void forgetTag(Tag tag);
  bool handleEvent(QObject* watched, QEvent* event);
  Tag hitTest(const QPointF& point) const;

 private:
  struct Entry {
    QPointer<QQuickItem> item;
    std::shared_ptr<const facebook::react::TouchEventEmitter> emitter;
    QMetaObject::Connection focusConnection;
    facebook::react::PointerEventsMode pointerEvents{};
    bool focusable{false};
    bool disabled{false};
    bool editable{false};
    bool keyHandler{false};
    bool modal{false};
  };
  Tag tagForItem(const QQuickItem* item) const;
  Tag focusTarget() const;
  std::vector<Tag> ancestry(Tag tag) const;
  QQuickItem* modalRoot() const;
  bool inModalScope(const QQuickItem* item) const;
  bool key(QKeyEvent& event);
  void hover(const QMouseEvent* event);
  void moveFocus(bool forward);
  QtQuickMountingLayer& layer_;
  QQuickItem& root_;
  QHash<Tag, Entry> entries_;
  std::vector<Tag> hovered_;
  QPointF lastPoint_;
  Tag keyPressTag_{-1};
  int keyPressCode_{0};
};

namespace desktop {
facebook::react::PointerEvent pointerEvent(
    const QMouseEvent* event, const QQuickItem& item, QPointF point);
void keyEvent(const facebook::react::TouchEventEmitter& emitter,
              Tag tag, const QKeyEvent& event);
void keyboardClick(const facebook::react::TouchEventEmitter& emitter,
                   Tag tag, const QKeyEvent& event);
}  // namespace desktop
}  // namespace rnqnx::fabric
