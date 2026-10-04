#pragma once

#include <QByteArray>
#include <QMap>
#include <QObject>
#include <QStringList>
#include <QUrl>

#include <functional>
#include <map>

class QWebSocket;
class QTimer;

namespace rnqnx::runtime {

// All methods and callbacks run on this QObject's thread. No JSI values cross
// the thread boundary. The TurboModule queues commands onto the Qt event loop.
class QtWebSocketTransport final : public QObject {
 public:
  struct Event {
    enum class Kind { Open, Message, Closed, Failed } kind;
    int id;
    QString data;
    QString protocol;
    bool binary{false};
    int code{0};
  };
  using Emit = std::function<void(Event)>;
  explicit QtWebSocketTransport(Emit emit, int connectTimeoutMs = 30000,
                                int closeTimeoutMs = 5000);
  ~QtWebSocketTransport() override;

  void connectSocket(int id, const QUrl& url, const QStringList& protocols,
                     const QMap<QByteArray, QByteArray>& headers);
  void sendText(int id, const QString& message);
  void sendBinary(int id, const QByteArray& base64);
  void ping(int id);
  void closeSocket(int id, int code, const QString& reason);
  std::size_t socketCount() const { return sockets_.size(); }

 private:
  struct Socket { QWebSocket* socket; QTimer* timer; bool closing{false}; bool opened{false}; };
  void fail(int id, const QString& message);
  void finish(int id, Event event);
  QWebSocket* writable(int id);
  Emit emit_;
  int connectTimeoutMs_;
  int closeTimeoutMs_;
  std::map<int, Socket> sockets_;
};

} // namespace rnqnx::runtime
