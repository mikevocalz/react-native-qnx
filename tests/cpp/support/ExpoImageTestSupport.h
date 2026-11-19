#pragma once

// Shared pieces of the ExpoImage host tests: a check that throws, PNG bytes, and
// a local HTTP server that answers every request with a PNG and remembers it.
#include <QBuffer>
#include <QColor>
#include <QByteArray>
#include <QHostAddress>
#include <QImage>
#include <QImageReader>
#include <QFile>
#include <QObject>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrl>

#include <stdexcept>
#include <string>

namespace expo_image_test {

inline void check(bool condition, const std::string& message) {
  if (!condition) throw std::runtime_error(message);
}

inline void writeFile(const QString& path, const QByteArray& bytes) {
  QFile file(path);
  check(file.open(QIODevice::WriteOnly), "cannot write " + path.toStdString());
  file.write(bytes);
}

inline QByteArray pngBytes(const QSize& size, const QColor& color) {
  QImage image(size, QImage::Format_ARGB32);
  image.fill(color);
  QByteArray bytes;
  QBuffer buffer(&bytes);
  buffer.open(QIODevice::WriteOnly);
  image.save(&buffer, "PNG");
  return bytes;
}

/// A two-frame, 1x1 animated GIF (red, then blue, 10 ms each) that loops
/// forever (NETSCAPE2.0 extension), so it keeps playing until it is paused.
inline QByteArray animatedGif() {
  static const unsigned char gif[] = {
      // header, screen descriptor, 4-entry global color table
      0x47, 0x49, 0x46, 0x38, 0x39, 0x61, 0x01, 0x00, 0x01, 0x00, 0xA1, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      // NETSCAPE2.0 application extension: loop count 0 (forever)
      0x21, 0xFF, 0x0B, 0x4E, 0x45, 0x54, 0x53, 0x43, 0x41, 0x50, 0x45, 0x32, 0x2E, 0x30, 0x03, 0x01, 0x00, 0x00, 0x00,
      // frame 1
      0x21, 0xF9, 0x04, 0x04, 0x01, 0x00, 0x00, 0x00, 0x2C, 0x00, 0x00, 0x00, 0x00,
      0x01, 0x00, 0x01, 0x00, 0x00, 0x02, 0x02, 0x44, 0x01, 0x00,
      // frame 2
      0x21, 0xF9, 0x04, 0x04, 0x01, 0x00, 0x01, 0x00, 0x2C,
      0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x02, 0x02, 0x4C, 0x01, 0x00, 0x3B};
  return QByteArray(reinterpret_cast<const char*>(gif), sizeof(gif));
}

inline std::string dataUri(const QByteArray& png) {
  return "data:image/png;base64," + png.toBase64().toStdString();
}

inline std::string fileUri(const QString& path) { return QUrl::fromLocalFile(path).toString().toStdString(); }

/// Answers every request with a PNG and keeps the request text.
class PngServer : public QObject {
 public:
  explicit PngServer(const QByteArray& png) : png_(png) {
    check(server_.listen(QHostAddress::LocalHost), "the test server could not listen");
    QObject::connect(&server_, &QTcpServer::newConnection, this, [this] {
      while (auto* socket = server_.nextPendingConnection()) {
        QObject::connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
          requests_.push_back(QString::fromLatin1(socket->readAll()));
          socket->write("HTTP/1.1 200 OK\r\nContent-Type: image/png\r\nConnection: close\r\nContent-Length: " +
                        QByteArray::number(png_.size()) + "\r\n\r\n");
          socket->write(png_);
          socket->disconnectFromHost();
        });
      }
    });
  }
  QString url(const QString& path = QStringLiteral("/a.png")) const {
    return QStringLiteral("http://127.0.0.1:%1%2").arg(server_.serverPort()).arg(path);
  }
  const QStringList& requests() const { return requests_; }

 private:
  QTcpServer server_;
  QByteArray png_;
  QStringList requests_;
};

}  // namespace expo_image_test
