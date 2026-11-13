// QtVideoItem on the offscreen platform with a 1 s generated VP8 clip.
// Covers: QML loads from the compiled-in resource, load/seek/end/error signals,
// source resolution, and that destroying the item destroys its QML player.
#include "QtVideoItem.h"

#include <QGuiApplication>
#include <QPointer>
#include <QQmlEngine>
#include <QMediaPlayer>
#include <QQuickItem>
#include <QSignalSpy>
#include <QTest>
#include <QUrl>

using namespace rnqnx::views;

class VideoItemTest : public QObject {
  Q_OBJECT

 private:
  // A machine whose Qt Multimedia has no working backend (for example a
  // Homebrew Qt with only the AVFoundation one, which does not report
  // status changes under the offscreen platform) cannot run playback tests.
  // Set RNQNX_QT_VIDEO_REQUIRE_PLAYBACK=1 to turn that skip into a failure.
  static bool playbackWorks() {
    QMediaPlayer probe;
    QSignalSpy errors(&probe, &QMediaPlayer::errorOccurred);
    probe.setSource(QUrl::fromLocalFile(QStringLiteral("/nonexistent/probe.mp4")));
    return errors.wait(5000);
  }

 private Q_SLOTS:
  void initTestCase() {
    m_playback = playbackWorks();
    if (!m_playback && qEnvironmentVariableIsSet("RNQNX_QT_VIDEO_REQUIRE_PLAYBACK"))
      QFAIL("Qt Multimedia playback is required but not working");
  }
  void resolvesSources() {
    const QString base = QStringLiteral("/opt/app");
    QCOMPARE(QtVideoItem::resolveSource(QStringLiteral("http://h/a.mp4"), base), QUrl("http://h/a.mp4"));
    QCOMPARE(QtVideoItem::resolveSource(QStringLiteral("rtsp://h/a"), base), QUrl("rtsp://h/a"));
    QCOMPARE(QtVideoItem::resolveSource(QStringLiteral("/tmp/a.mp4"), base), QUrl::fromLocalFile("/tmp/a.mp4"));
    QCOMPARE(QtVideoItem::resolveSource(QStringLiteral("assets/a.mp4"), base),
             QUrl::fromLocalFile("/opt/app/assets/a.mp4"));
    QVERIFY(QtVideoItem::resolveSource(QString(), base).isEmpty());
  }

  void playsSeeksAndEnds() {
    if (!m_playback) QSKIP("no working Qt Multimedia backend");
    QQmlEngine engine;
    QtVideoItem item(engine);
    QVERIFY2(item.isReady(), qPrintable(item.errorString()));
    item.setSize(QSizeF(160, 90));
    QSignalSpy loaded(&item, &QtVideoItem::videoLoaded);
    QSignalSpy ended(&item, &QtVideoItem::videoEnded);
    QSignalSpy seeked(&item, &QtVideoItem::videoSeeked);
    QSignalSpy failed(&item, &QtVideoItem::videoFailed);
    item.setSource(QStringLiteral(RNQNX_QT_VIDEO_CLIP));
    QVERIFY2(loaded.wait(15000), "videoLoaded");
    const auto load = loaded.takeFirst();
    QVERIFY(load.at(0).toDouble() > 0.5 && load.at(0).toDouble() < 1.6);
    QCOMPARE(load.at(1).toInt(), 160);
    QCOMPARE(load.at(2).toInt(), 90);
    item.seek(0.5);
    QVERIFY2(seeked.wait(5000), "videoSeeked");
    QCOMPARE(seeked.first().at(1).toDouble(), 0.5);
    QVERIFY2(ended.wait(15000), "videoEnded");
    QVERIFY(failed.isEmpty());
  }

  void reportsBadSource() {
    if (!m_playback) QSKIP("no working Qt Multimedia backend");
    QQmlEngine engine;
    QtVideoItem item(engine);
    QVERIFY(item.isReady());
    QSignalSpy failed(&item, &QtVideoItem::videoFailed);
    item.setSource(QStringLiteral("/nonexistent/none.mp4"));
    QVERIFY2(failed.wait(15000), "videoFailed");
    QVERIFY(!failed.first().at(0).toString().isEmpty());
  }

  void destroysItsPlayer() {
    QQmlEngine engine;
    auto* item = new QtVideoItem(engine);
    QVERIFY(item->isReady());
    const auto children = item->childItems();
    QCOMPARE(children.size(), 1);
    QPointer<QQuickItem> inner = children.first();
    delete item;
    QVERIFY2(inner.isNull(), "the QML player must not outlive its item");
  }

 private:
  bool m_playback{false};
};

QTEST_MAIN(VideoItemTest)
#include "VideoItemTest.moc"
