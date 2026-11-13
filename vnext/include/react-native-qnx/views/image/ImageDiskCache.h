#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QDir>
#include <QMutex>
#include <QString>

#include <cstdint>

namespace rnqnx::views {

/// Encoded images on disk, bounded by a byte budget with least-recently-used
/// eviction. One file per key, named by the key's SHA-256; a file's
/// modification time is its last use, so the index is the directory itself and
/// nothing has to be rebuilt after a restart. All members are thread-safe.
class ImageDiskCache {
 public:
  /// \a directory is created on first write.
  ImageDiskCache(QString directory, std::int64_t budgetBytes);

  /// The process-wide cache under the platform cache location.
  static ImageDiskCache& shared();

  /// The cached bytes for \a key, or empty. A hit refreshes the entry's age.
  QByteArray read(const QString& key);
  /// Stores \a bytes, then evicts the oldest entries until the budget holds.
  /// An entry larger than the whole budget is not stored (returns false).
  bool write(const QString& key, const QByteArray& bytes);
  bool contains(const QString& key) const;
  /// The file that holds \a key (it may not exist).
  QString pathFor(const QString& key) const;
  /// Removes every entry.
  void clear();
  std::int64_t usedBytes() const;
  std::int64_t budgetBytes() const { return budget_; }
  void setBudgetBytes(std::int64_t bytes);
  QString directory() const { return directory_; }

 private:
  void evictLocked();

  QString directory_;
  std::int64_t budget_;
  mutable QMutex mutex_;
};

}  // namespace rnqnx::views
