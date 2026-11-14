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
#include <atomic>
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

struct ImageDiskCache::State {
  State(QString path, std::int64_t bytes) : directory(std::move(path)), budget(bytes) {}
  const QString directory;
  std::atomic<std::int64_t> budget;
  QMutex mutex;
  std::uint64_t generation{0};
};

ImageDiskCache::ImageDiskCache(QString directory, std::int64_t budgetBytes)
    : state_(std::make_shared<State>(std::move(directory), std::max<std::int64_t>(0, budgetBytes))) {}

std::int64_t ImageDiskCache::budgetBytes() const { return state_->budget.load(); }
QString ImageDiskCache::directory() const { return state_->directory; }

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
  return state_->directory + QLatin1Char('/') + QString::fromLatin1(digest) + QLatin1String(kSuffix);
}

QByteArray ImageDiskCache::read(const QString& key, qint64 maxBytes) {
  QMutexLocker lock(&state_->mutex);
  QFile file(pathFor(key));
  if (!file.open(QIODevice::ReadOnly)) return {};
  if (maxBytes < 0 || file.size() > maxBytes) {
    file.close();
    QFile::remove(pathFor(key));
    return {};
  }
  QByteArray bytes = maxBytes == std::numeric_limits<qint64>::max()
      ? file.readAll() : file.read(maxBytes + 1);
  if (bytes.size() > maxBytes) {
    file.close();
    QFile::remove(pathFor(key));
    return {};
  }
  file.close();
  // Touch: a hit makes the entry the most recently used.
  QFile touch(pathFor(key));
  if (touch.open(QIODevice::ReadWrite)) {
    touch.setFileTime(QDateTime::currentDateTimeUtc(), QFileDevice::FileModificationTime);
  }
  return bytes;
}

void ImageDiskCache::remove(const QString& key) {
  QMutexLocker lock(&state_->mutex);
  QFile::remove(pathFor(key));
}

bool ImageDiskCache::write(const QString& key, const QByteArray& bytes) {
  return write(key, bytes, generation());
}

std::uint64_t ImageDiskCache::generation() const {
  QMutexLocker lock(&state_->mutex);
  return state_->generation;
}

bool ImageDiskCache::write(const QString& key, const QByteArray& bytes,
                           std::uint64_t generation) {
  QMutexLocker lock(&state_->mutex);
  if (generation != state_->generation) return false;
  if (bytes.isEmpty() || bytes.size() > state_->budget) return false;
  if (!QDir().mkpath(state_->directory)) return false;
  QSaveFile file(pathFor(key));
  if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
    return false;
  }
  evictLocked();
  return true;
}

bool ImageDiskCache::contains(const QString& key) const {
  QMutexLocker lock(&state_->mutex);
  return QFileInfo::exists(pathFor(key));
}

void ImageDiskCache::clear() {
  QMutexLocker lock(&state_->mutex);
  ++state_->generation;
  QDirIterator it(state_->directory, {QStringLiteral("*") + QLatin1String(kSuffix)}, QDir::Files);
  while (it.hasNext()) QFile::remove(it.next());
}

std::int64_t ImageDiskCache::usedBytes() const {
  QMutexLocker lock(&state_->mutex);
  std::int64_t total = 0;
  QDirIterator it(state_->directory, {QStringLiteral("*") + QLatin1String(kSuffix)}, QDir::Files);
  while (it.hasNext()) total += QFileInfo(it.next()).size();
  return total;
}

void ImageDiskCache::setBudgetBytes(std::int64_t bytes) {
  QMutexLocker lock(&state_->mutex);
  state_->budget = std::max<std::int64_t>(0, bytes);
  evictLocked();
}

void ImageDiskCache::evictLocked() {
  std::vector<Entry> entries;
  std::int64_t total = 0;
  QDirIterator it(state_->directory, {QStringLiteral("*") + QLatin1String(kSuffix)}, QDir::Files);
  while (it.hasNext()) {
    const QFileInfo info(it.next());
    entries.push_back({info.absoluteFilePath(), info.size(), info.lastModified()});
    total += info.size();
  }
  if (total <= state_->budget) return;
  std::sort(entries.begin(), entries.end(),
            [](const Entry& a, const Entry& b) { return a.used < b.used; });
  for (const Entry& entry : entries) {
    if (total <= state_->budget) break;
    if (QFile::remove(entry.path)) total -= entry.size;
  }
}

}  // namespace rnqnx::views
