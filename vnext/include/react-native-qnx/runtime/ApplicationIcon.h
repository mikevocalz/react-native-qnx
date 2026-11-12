#pragma once

#include <QDir>
#include <QGuiApplication>
#include <QIcon>
#include <QImage>
#include <QJsonArray>
#include <QList>
#include <QString>
#include <stdexcept>

namespace rnqnx::runtime {

// Loads the staged icon-<size>.png set that prepare-native generated and the
// consumer build copied next to the executable. Every listed size must decode
// to a square image of exactly that edge; a partial set is an error, not a
// silent fallback to Qt's default icon.
inline QIcon loadApplicationIcon(const QString& directory, const QList<int>& sizes) {
  if (sizes.isEmpty()) throw std::invalid_argument("The application icon set is empty.");
  QIcon icon;
  for (const int size : sizes) {
    const QString path = QDir(directory).filePath(QStringLiteral("icon-%1.png").arg(size));
    const QImage image(path, "png");
    if (image.isNull() || image.width() != size || image.height() != size) {
      throw std::invalid_argument(
          ("Application icon " + path + " is missing or not " + QString::number(size) + " px square.")
              .toStdString());
    }
    icon.addFile(path, QSize(size, size));
  }
  return icon;
}

// Sets the default icon for every window (title bar and task switcher on
// platforms whose window manager reads it). Launcher tiles come from the
// platform's own app metadata, not from this call.
inline void applyApplicationIcon(const QIcon& icon) {
  QGuiApplication::setWindowIcon(icon);
}

inline QJsonArray describeApplicationIconSizes() {
  QJsonArray sizes;
  for (const auto& size : QGuiApplication::windowIcon().availableSizes()) sizes.append(size.width());
  return sizes;
}

} // namespace rnqnx::runtime
