#include "react-native-qnx/views/image/ImageDiskCache.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QMutexLocker>
#include <QSaveFile>
#include <QStandardPaths>

#include <algorithm>
#include <vector>

namespace rnqnx::views {
namespace {

constexpr std::int64_t kDefaultBudgetBytes = 128LL * 1024 * 1024;
constexpr auto kSuffix = ".img";

struct Entry {
  QString path;
  qint64 size;
  QDateTime used;
};

}  // namespace

ImageDiskCache::ImageDiskCache(QString directory, std::int64_t budgetBytes)
    : directory_(std::move(directory)), budget_(budgetBytes) {}

ImageDiskCache& ImageDiskCache::shared() {
  static ImageDiskCache cache = [] {
    QString base = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (base.isEmpty()) base = QDir::tempPath() + QStringLiteral("/rnqnx-cache");
    return ImageDiskCache(base + QStringLiteral("/images"), kDefaultBudgetBytes);
  }();
  return cache;
}

QString ImageDiskCache::pathFor(const QString& key) const {
  const auto digest =
      QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha256).toHex();
  return directory_ + QLatin1Char('/') + QString::fromLatin1(digest) + QLatin1String(kSuffix);
}

QByteArray ImageDiskCache::read(const QString& key) {
  QMutexLocker lock(&mutex_);
  QFile file(pathFor(key));
  if (!file.open(QIODevice::ReadOnly)) return {};
  QByteArray bytes = file.readAll();
  file.close();
  // Touch: a hit makes the entry the most recently used.
  QFile touch(pathFor(key));
  if (touch.open(QIODevice::ReadWrite)) {
    touch.setFileTime(QDateTime::currentDateTimeUtc(), QFileDevice::FileModificationTime);
  }
  return bytes;
}

bool ImageDiskCache::write(const QString& key, const QByteArray& bytes) {
  QMutexLocker lock(&mutex_);
  if (bytes.isEmpty() || bytes.size() > budget_) return false;
  if (!QDir().mkpath(directory_)) return false;
  QSaveFile file(pathFor(key));
  if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
    return false;
  }
  evictLocked();
  return true;
}

bool ImageDiskCache::contains(const QString& key) const {
  QMutexLocker lock(&mutex_);
  return QFileInfo::exists(pathFor(key));
}

void ImageDiskCache::clear() {
  QMutexLocker lock(&mutex_);
  QDirIterator it(directory_, {QStringLiteral("*") + QLatin1String(kSuffix)}, QDir::Files);
  while (it.hasNext()) QFile::remove(it.next());
}

std::int64_t ImageDiskCache::usedBytes() const {
  QMutexLocker lock(&mutex_);
  std::int64_t total = 0;
  QDirIterator it(directory_, {QStringLiteral("*") + QLatin1String(kSuffix)}, QDir::Files);
  while (it.hasNext()) total += QFileInfo(it.next()).size();
  return total;
}

void ImageDiskCache::setBudgetBytes(std::int64_t bytes) {
  QMutexLocker lock(&mutex_);
  budget_ = bytes;
  evictLocked();
}

void ImageDiskCache::evictLocked() {
  std::vector<Entry> entries;
  std::int64_t total = 0;
  QDirIterator it(directory_, {QStringLiteral("*") + QLatin1String(kSuffix)}, QDir::Files);
  while (it.hasNext()) {
    const QFileInfo info(it.next());
    entries.push_back({info.absoluteFilePath(), info.size(), info.lastModified()});
    total += info.size();
  }
  if (total <= budget_) return;
  std::sort(entries.begin(), entries.end(),
            [](const Entry& a, const Entry& b) { return a.used < b.used; });
  for (const Entry& entry : entries) {
    if (total <= budget_) break;
    if (QFile::remove(entry.path)) total -= entry.size;
  }
}

}  // namespace rnqnx::views
