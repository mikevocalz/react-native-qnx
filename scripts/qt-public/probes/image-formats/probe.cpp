// Format probe: lists QImageReader/QImageWriter formats and decodes each file given as an argument.
#include <QBuffer>
#include <QGuiApplication>
#include <QFile>
#include <QImageReader>
#include <QMovie>
#include <QTimer>
#include <cstdio>

int main(int argc, char** argv) {
  QGuiApplication app(argc, argv);
  QStringList names;
  for (const QByteArray& f : QImageReader::supportedImageFormats()) names << QString::fromLatin1(f);
  std::printf("supportedImageFormats: %s\n", qPrintable(names.join(' ')));
  QStringList mt;
  for (const QByteArray& f : QImageReader::supportedMimeTypes()) mt << QString::fromLatin1(f);
  std::printf("supportedMimeTypes: %s\n", qPrintable(mt.join(' ')));
  for (int i = 1; i < argc; ++i) {
    QFile file(argv[i]);
    file.open(QIODevice::ReadOnly);
    QByteArray bytes = file.readAll();
    QBuffer buf(&bytes);
    buf.open(QIODevice::ReadOnly);
    QImageReader r(&buf);
    const bool anim = r.supportsAnimation();
    const int count = r.imageCount();
    std::printf("%s: format=%s size=%dx%d supportsAnimation=%d imageCount=%d\n", argv[i],
                r.format().constData(), r.size().width(), r.size().height(), anim, count);
    // Every frame through QImageReader: distinct pixel colours prove distinct frames.
    {
      QBuffer rbuf(&bytes);
      rbuf.open(QIODevice::ReadOnly);
      QImageReader fr(&rbuf);
      for (int f = 0; f < fr.imageCount(); ++f) {
        fr.jumpToImage(f);
        QImage im = fr.read().convertToFormat(QImage::Format_ARGB32);
        std::printf("  reader frame %d: %dx%d px(2,2)=%08x delay=%dms\n", f, im.width(), im.height(),
                    im.isNull() ? 0u : im.pixel(2, 2), fr.nextImageDelay());
      }
    }
    // The same through QMovie, which ExpoImageItem uses.
    QBuffer mbuf(&bytes);
    mbuf.open(QIODevice::ReadOnly);
    QMovie m(&mbuf);
    m.setCacheMode(QMovie::CacheNone);
    std::printf("  QMovie valid=%d frameCount=%d\n", m.isValid(), m.frameCount());
    if (m.frameCount() > 1) {
      // Same call sequence as ExpoImageItem: start() and read currentImage() on frameChanged.
      QObject::connect(&m, &QMovie::frameChanged, &app, [&m](int n) {
        const QImage im = m.currentImage().convertToFormat(QImage::Format_ARGB32);
        std::printf("  frameChanged %d: null=%d px(2,2)=%08x\n", n, im.isNull(), im.isNull() ? 0u : im.pixel(2, 2));
      });
      m.start();
      QTimer::singleShot(450, &app, &QGuiApplication::quit);
      app.exec();
      m.stop();
    }
  }
}
