// The image fetch queue and the fetcher that uses it: ordering by expo-image's
// `priority` and the bound on loads in flight, against a real local HTTP server.
#include "react-native-qnx/views/image/ExpoImageItem.h"
#include "react-native-qnx/views/image/ImageFetchQueue.h"
#include "react-native-qnx/views/image/QtImageFetcher.h"

#include <QGuiApplication>
#include <QHostAddress>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest>

#include <algorithm>
#include <memory>
#include <vector>

using namespace rnqnx::views;

namespace {

// A server that accepts connections and answers none until released, so the
// test controls how many requests are in flight.
class HoldingServer : public QObject {
  Q_OBJECT

 public:
  HoldingServer() {
    QVERIFY(server_.listen(QHostAddress::LocalHost));
    connect(&server_, &QTcpServer::newConnection, this, [this] {
      while (auto* socket = server_.nextPendingConnection()) {
        connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
          const QByteArray data = socket->readAll();
          const int begin = data.indexOf("GET /");
          if (begin < 0) return;
          const int end = data.indexOf(' ', begin + 4);
          arrived_.push_back(QString::fromLatin1(data.mid(begin + 4, end - begin - 4)));
          held_.push_back(socket);
        });
      }
    });
  }
  QString url(const QString& path) const {
    return QStringLiteral("http://127.0.0.1:%1%2").arg(server_.serverPort()).arg(path);
  }
  const QStringList& arrived() const { return arrived_; }
  int heldCount() const { return static_cast<int>(held_.size()); }
  // Answers the oldest held request with a one-pixel PNG.
  void answerOne() {
    QTcpSocket* socket = held_.front();
    held_.erase(held_.begin());
    QImage image(1, 1, QImage::Format_ARGB32);
    image.fill(Qt::red);
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    socket->write("HTTP/1.1 200 OK\r\nContent-Type: image/png\r\nConnection: close\r\nContent-Length: " +
                  QByteArray::number(png.size()) + "\r\n\r\n" + png);
    socket->disconnectFromHost();
  }

 private:
  QTcpServer server_;
  QStringList arrived_;
  std::vector<QTcpSocket*> held_;
};

ImageRequest requestFor(const QString& url, ImagePriority priority) {
  ImageRequest request;
  request.uri = url;
  request.policy = ImageCachePolicy::None;
  request.priority = priority;
  return request;
}

}  // namespace

class ImageFetchQueueTest : public QObject {
  Q_OBJECT

 private Q_SLOTS:
  void parsesPriorityNames() {
    QCOMPARE(parseImagePriority(QStringLiteral("low")), ImagePriority::Low);
    QCOMPARE(parseImagePriority(QStringLiteral("normal")), ImagePriority::Normal);
    QCOMPARE(parseImagePriority(QStringLiteral("high")), ImagePriority::High);
    QCOMPARE(parseImagePriority(QString()), ImagePriority::Normal);
    QCOMPARE(parseImagePriority(QStringLiteral("urgent")), ImagePriority::Normal);
  }

  void startsAtMostMaxConcurrentTasks() {
    ImageFetchQueue queue(2);
    int started = 0;
    for (int i = 0; i < 5; ++i) queue.submit(ImagePriority::Normal, [&] { ++started; });
    QCOMPARE(started, 2);
    QCOMPARE(queue.running(), 2);
    QCOMPARE(queue.waiting(), 3);
    queue.release();
    QCOMPARE(started, 3);
    QCOMPARE(queue.running(), 2);
    QVERIFY(queue.running() <= queue.maxConcurrent());
  }

  void startsWaitingTasksHighestPriorityFirst() {
    ImageFetchQueue queue(1);
    std::vector<QString> order;
    queue.submit(ImagePriority::Normal, [&] { order.push_back(QStringLiteral("first")); });
    queue.submit(ImagePriority::Low, [&] { order.push_back(QStringLiteral("low")); });
    queue.submit(ImagePriority::Normal, [&] { order.push_back(QStringLiteral("normal-a")); });
    queue.submit(ImagePriority::High, [&] { order.push_back(QStringLiteral("high")); });
    queue.submit(ImagePriority::Normal, [&] { order.push_back(QStringLiteral("normal-b")); });
    for (int i = 0; i < 4; ++i) queue.release();
    const std::vector<QString> expected{QStringLiteral("first"), QStringLiteral("high"),
                                        QStringLiteral("normal-a"), QStringLiteral("normal-b"),
                                        QStringLiteral("low")};
    QVERIFY(order == expected);
  }

  void cancelDropsAWaitingTaskOnly() {
    ImageFetchQueue queue(1);
    int ran = 0;
    const auto running = queue.submit(ImagePriority::Normal, [&] { ++ran; });
    const auto waiting = queue.submit(ImagePriority::Normal, [&] { ++ran; });
    QVERIFY(!queue.cancel(running));
    QVERIFY(queue.cancel(waiting));
    QVERIFY(!queue.cancel(waiting));
    queue.release();
    QCOMPARE(ran, 1);
    QCOMPARE(queue.running(), 0);
  }

  void aTaskThatFinishesAtOnceDoesNotStallTheQueue() {
    ImageFetchQueue queue(1);
    int ran = 0;
    for (int i = 0; i < 4; ++i) {
      queue.submit(ImagePriority::Normal, [&] {
        ++ran;
        queue.release();
      });
    }
    QCOMPARE(ran, 4);
    QCOMPARE(queue.running(), 0);
  }

  void fetcherKeepsLoadsInFlightWithinTheBound() {
    HoldingServer server;
    QtImageFetcher fetcher(5000, nullptr, nullptr, 2);
    int finished = 0;
    for (int i = 0; i < 5; ++i) {
      fetcher.fetch(requestFor(server.url(QStringLiteral("/%1").arg(i)), ImagePriority::Normal), {},
                    [&](FetchResult) { ++finished; });
    }
    QTRY_COMPARE_WITH_TIMEOUT(server.heldCount(), 2, 5000);
    QCOMPARE(fetcher.activeRequests(), 2);
    QCOMPARE(fetcher.queuedRequests(), 3);
    for (int answered = 0; answered < 5; ++answered) {
      QVERIFY(fetcher.activeRequests() <= 2);
      QTRY_VERIFY_WITH_TIMEOUT(server.heldCount() > 0, 5000);
      server.answerOne();
      QTRY_COMPARE_WITH_TIMEOUT(finished, answered + 1, 5000);
    }
    QCOMPARE(fetcher.activeRequests(), 0);
    QCOMPARE(fetcher.queuedRequests(), 0);
  }

  void fetcherStartsHigherPriorityRequestsFirst() {
    HoldingServer server;
    QtImageFetcher fetcher(5000, nullptr, nullptr, 1);
    int finished = 0;
    const auto done = [&](FetchResult) { ++finished; };
    fetcher.fetch(requestFor(server.url(QStringLiteral("/occupies")), ImagePriority::Normal), {}, done);
    QTRY_COMPARE_WITH_TIMEOUT(server.heldCount(), 1, 5000);
    fetcher.fetch(requestFor(server.url(QStringLiteral("/low")), ImagePriority::Low), {}, done);
    fetcher.fetch(requestFor(server.url(QStringLiteral("/normal")), ImagePriority::Normal), {}, done);
    fetcher.fetch(requestFor(server.url(QStringLiteral("/high")), ImagePriority::High), {}, done);
    QCOMPARE(fetcher.queuedRequests(), 3);
    for (int answered = 0; answered < 4; ++answered) {
      QTRY_VERIFY_WITH_TIMEOUT(server.heldCount() > 0, 5000);
      server.answerOne();
      QTRY_COMPARE_WITH_TIMEOUT(finished, answered + 1, 5000);
    }
    const QStringList expected{QStringLiteral("/occupies"), QStringLiteral("/high"), QStringLiteral("/normal"),
                               QStringLiteral("/low")};
    QCOMPARE(server.arrived(), expected);
  }

  // expo-image's `priority` prop orders loads across views, because every view
  // loads through the one shared queue.
  void viewsLoadInPriorityOrderThroughTheSharedQueue() {
    ImageFetchQueue& shared = ImageFetchQueue::shared();
    QCOMPARE(shared.running(), 0);
    HoldingServer server;
    const int slots = ImageFetchQueue::kSharedConcurrency;
    std::vector<std::unique_ptr<ExpoImageItem>> views;
    const auto addView = [&](const QString& path, const QString& priority) {
      auto view = std::make_unique<ExpoImageItem>();
      view->setSize(QSizeF(10, 10));
      view->setCachePolicy(QStringLiteral("none"));
      view->setPriority(priority);
      view->setSource({QVariantMap{{QStringLiteral("uri"), server.url(path)}}});
      views.push_back(std::move(view));
    };
    for (int i = 0; i < slots; ++i) addView(QStringLiteral("/fill%1").arg(i), QStringLiteral("normal"));
    QTRY_COMPARE_WITH_TIMEOUT(server.heldCount(), slots, 5000);
    QCOMPARE(shared.running(), slots);
    addView(QStringLiteral("/low"), QStringLiteral("low"));
    addView(QStringLiteral("/normal"), QStringLiteral("normal"));
    addView(QStringLiteral("/high"), QStringLiteral("high"));
    addView(QStringLiteral("/default"), QString());  // no priority prop: normal
    QTRY_COMPARE_WITH_TIMEOUT(shared.waiting(), 4, 5000);
    QCOMPARE(shared.running(), slots);  // never more than the bound
    for (int answered = 0; answered < slots + 4; ++answered) {
      QTRY_VERIFY_WITH_TIMEOUT(server.heldCount() > 0, 5000);
      QVERIFY(shared.running() <= slots);
      server.answerOne();
      QTest::qWait(30);
    }
    const QStringList arrived = server.arrived();
    QCOMPARE(arrived.size(), slots + 4);
    const QStringList afterTheFill = arrived.mid(slots);
    const QStringList expected{QStringLiteral("/high"), QStringLiteral("/normal"), QStringLiteral("/default"),
                               QStringLiteral("/low")};
    QCOMPARE(afterTheFill, expected);
    views.clear();
    QTRY_COMPARE_WITH_TIMEOUT(shared.running(), 0, 5000);
    QCOMPARE(shared.waiting(), 0);
  }

  void cancellingAQueuedLoadNeverStartsIt() {
    HoldingServer server;
    QtImageFetcher fetcher(5000, nullptr, nullptr, 1);
    int finished = 0;
    fetcher.fetch(requestFor(server.url(QStringLiteral("/a")), ImagePriority::Normal), {},
                  [&](FetchResult) { ++finished; });
    QTRY_COMPARE_WITH_TIMEOUT(server.heldCount(), 1, 5000);
    const auto doomed = fetcher.fetch(requestFor(server.url(QStringLiteral("/b")), ImagePriority::High), {},
                                      [&](FetchResult) { ++finished; });
    fetcher.cancel(doomed);
    QCOMPARE(fetcher.queuedRequests(), 0);
    server.answerOne();
    QTRY_COMPARE_WITH_TIMEOUT(finished, 1, 5000);
    QTest::qWait(100);
    QCOMPARE(server.arrived(), QStringList{QStringLiteral("/a")});
    QCOMPARE(finished, 1);
  }

  void cancellingARunningLoadFreesItsSlot() {
    HoldingServer server;
    QtImageFetcher fetcher(5000, nullptr, nullptr, 1);
    int finished = 0;
    const auto first = fetcher.fetch(requestFor(server.url(QStringLiteral("/a")), ImagePriority::Normal), {},
                                     [&](FetchResult) { ++finished; });
    fetcher.fetch(requestFor(server.url(QStringLiteral("/b")), ImagePriority::Normal), {},
                  [&](FetchResult) { ++finished; });
    QTRY_COMPARE_WITH_TIMEOUT(server.heldCount(), 1, 5000);
    fetcher.cancel(first);
    QTRY_COMPARE_WITH_TIMEOUT(server.arrived().size(), 2, 5000);
    QCOMPARE(server.arrived().back(), QStringLiteral("/b"));
    QCOMPARE(fetcher.activeRequests(), 1);
  }
};

int main(int argc, char** argv) {
  QGuiApplication app(argc, argv);
  ImageFetchQueueTest test;
  return QTest::qExec(&test, argc, argv);
}

#include "ImageFetchQueueTest.moc"
