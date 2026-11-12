#include "IconSet.h"

#include <QDir>
#include <QImage>
#include <QImageReader>
#include <QPainter>
#include <QSaveFile>
#include <stdexcept>

namespace rnqnx::identity {
namespace {

[[noreturn]] void reject(const QString& message) {
  throw std::runtime_error(message.toStdString());
}

} // namespace

QList<GeneratedIcon> writeIconSet(
    const QString& sourcePng, const QString& outputDirectory, const QList<int>& sizes) {
  // Decide by content, not by file extension.
  if (QImageReader::imageFormat(sourcePng) != "png") {
    reject(sourcePng + " is not a PNG. Export the icon as a square PNG of at least 1024x1024.");
  }
  QImageReader reader(sourcePng, "png");
  const QSize declared = reader.size();
  if (declared.width() != declared.height() || declared.width() < kMinimumIconSourcePx) {
    reject(QStringLiteral("%1 is %2x%3. Export a square PNG of at least %4x%4.")
               .arg(sourcePng)
               .arg(declared.width())
               .arg(declared.height())
               .arg(kMinimumIconSourcePx));
  }
  QImage source = reader.read();
  if (source.isNull()) reject(sourcePng + " cannot be decoded: " + reader.errorString());
  source = source.convertToFormat(QImage::Format_ARGB32_Premultiplied);
  if (!QDir().mkpath(outputDirectory)) reject("Cannot create " + outputDirectory);

  QList<GeneratedIcon> written;
  for (const int size : sizes) {
    if (size < 1 || size > source.width()) {
      reject(QStringLiteral("Icon size %1 is outside 1-%2.").arg(size).arg(source.width()));
    }
    const QImage scaled =
        source.scaled(size, size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    // Redraw into a fresh image so no source metadata (text chunks, DPI)
    // reaches the output and identical pixels produce identical files.
    QImage output(size, size, QImage::Format_ARGB32_Premultiplied);
    output.fill(Qt::transparent);
    {
      QPainter painter(&output);
      painter.setCompositionMode(QPainter::CompositionMode_Source);
      painter.drawImage(0, 0, scaled);
    }
    output = output.convertToFormat(QImage::Format_ARGB32);
    const QString path =
        QDir(outputDirectory).filePath(QStringLiteral("icon-%1.png").arg(size));
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || !output.save(&file, "png") || !file.commit()) {
      reject("Cannot write " + path);
    }
    written.append({size, path});
  }
  return written;
}

} // namespace rnqnx::identity
