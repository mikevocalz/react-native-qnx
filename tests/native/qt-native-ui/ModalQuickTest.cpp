// Drives the Modal item and its focus bridge with Qt key events in an offscreen
// window. A counter on the covered app item shows whether a key got past the
// modal; the callback passed to QtModalBridge stands in for the Fabric
// onRequestClose emitter.

#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtTest>

#include <memory>

#include "react-native-qnx/fabric/QtModalBridge.h"
#include "react-native-qnx/views/QtModalQml.h"

namespace {

using rnqnx::fabric::QtModalBridge;

constexpr const char* kAppItemQml = R"QML(import QtQuick
Item {
  property int escapes: 0
  property int tabs: 0
  activeFocusOnTab: true
  Keys.onEscapePressed: function(event) { escapes += 1; event.accepted = true }
  Keys.onTabPressed: function(event) { tabs += 1; event.accepted = true }
}
)QML";

constexpr const char* kFieldQml = R"QML(import QtQuick
TextInput { property int presses: 0 }
)QML";

class ModalQuickTest : public QObject {
  Q_OBJECT

 private:
  QQmlEngine engine_;
  std::unique_ptr<QQuickWindow> window_;
  int closes_{0};

  QQuickItem* create(const char* source, QQuickItem* parent) {
    QQmlComponent component(&engine_);
    component.setData(source, QUrl());
    auto* item = qobject_cast<QQuickItem*>(component.create());
    if (item == nullptr) {
      qWarning().noquote() << component.errorString();
      return nullptr;
    }
    item->setParentItem(parent);
    item->setSize(QSizeF(200, 100));
    return item;
  }

  QQuickItem* appItem() { return create(kAppItemQml, window_->contentItem()); }

  QQuickItem* openModal(int* closes) {
    QQmlComponent component(&engine_);
    component.setData(rnqnx::views::modalQml(), QUrl());
    auto* item = qobject_cast<QQuickItem*>(component.create());
    if (item == nullptr) {
      qWarning().noquote() << component.errorString();
      return nullptr;
    }
    item->setSize(QSizeF(800, 500));
    new QtModalBridge(*item, [closes] { *closes += 1; });
    item->setParentItem(window_->contentItem());
    return item;
  }

 private Q_SLOTS:
  void initTestCase() {
    window_ = std::make_unique<QQuickWindow>();
    window_->resize(800, 500);
    window_->show();
    QVERIFY(QTest::qWaitForWindowExposed(window_.get()));
    QVERIFY(QTest::qWaitForWindowActive(window_.get()));
  }
  void cleanupTestCase() { window_.reset(); }
  void init() { closes_ = 0; }

  void escapeRequestsCloseAndStopsAtTheModal() {
    QQuickItem* app = appItem();
    app->forceActiveFocus();
    QVERIFY(app->hasActiveFocus());

    QQuickItem* modal = openModal(&closes_);
    QVERIFY(modal->hasActiveFocus());

    QTest::keyClick(window_.get(), Qt::Key_Escape);

    QCOMPARE(closes_, 1);
    QCOMPARE(app->property("escapes").toInt(), 0);
    delete modal;
    delete app;
  }

  void escapeFromAFocusedFieldInsideTheModalReachesTheModal() {
    QQuickItem* modal = openModal(&closes_);
    QQuickItem* field = create(kFieldQml, modal);
    field->forceActiveFocus();
    QVERIFY(field->hasActiveFocus());

    QTest::keyClick(window_.get(), Qt::Key_Escape);

    QCOMPARE(closes_, 1);
    delete modal;
  }

  void otherKeysDoNotRequestClose() {
    QQuickItem* modal = openModal(&closes_);
    QTest::keyClick(window_.get(), Qt::Key_A);
    QTest::keyClick(window_.get(), Qt::Key_Return);
    QCOMPARE(closes_, 0);
    delete modal;
  }

  void focusStaysInTheModalWhenTheAppItemTriesToTakeIt() {
    QQuickItem* app = appItem();
    QQuickItem* modal = openModal(&closes_);
    QVERIFY(modal->hasActiveFocus());

    app->forceActiveFocus();

    QVERIFY(!app->hasActiveFocus());
    QVERIFY(modal->hasActiveFocus());
    QTest::keyClick(window_.get(), Qt::Key_Tab);
    QCOMPARE(app->property("tabs").toInt(), 0);
    delete modal;
    delete app;
  }

  void focusReturnsToThePreviousItemWhenTheModalCloses() {
    QQuickItem* app = appItem();
    app->forceActiveFocus();
    QQuickItem* modal = openModal(&closes_);
    QVERIFY(!app->hasActiveFocus());

    delete modal;

    QVERIFY(app->hasActiveFocus());
    QTest::keyClick(window_.get(), Qt::Key_Escape);
    QCOMPARE(app->property("escapes").toInt(), 1);
    QCOMPARE(closes_, 0);
    delete app;
  }

  void aModalOpenedAboveAnotherTakesEscapeAndHandsFocusBackOnClose() {
    QQuickItem* app = appItem();
    app->forceActiveFocus();
    int lowerCloses = 0;
    QQuickItem* lower = openModal(&lowerCloses);
    int upperCloses = 0;
    QQuickItem* upper = openModal(&upperCloses);
    QVERIFY(upper->hasActiveFocus());

    QTest::keyClick(window_.get(), Qt::Key_Escape);
    QCOMPARE(upperCloses, 1);
    QCOMPARE(lowerCloses, 0);

    delete upper;
    QVERIFY(lower->hasActiveFocus());
    QTest::keyClick(window_.get(), Qt::Key_Escape);
    QCOMPARE(lowerCloses, 1);

    delete lower;
    QVERIFY(app->hasActiveFocus());
    delete app;
  }

  void aModalClosedBelowAnotherLeavesTheUpperModalFocused() {
    QQuickItem* lower = openModal(&closes_);
    int upperCloses = 0;
    QQuickItem* upper = openModal(&upperCloses);

    delete lower;

    QVERIFY(upper->hasActiveFocus());
    QTest::keyClick(window_.get(), Qt::Key_Escape);
    QCOMPARE(upperCloses, 1);
    delete upper;
  }
};

}  // namespace

QTEST_MAIN(ModalQuickTest)
#include "ModalQuickTest.moc"
