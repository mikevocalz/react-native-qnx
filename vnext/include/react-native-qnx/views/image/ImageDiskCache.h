#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QDir>
#include <QMutex>
#include <QString>

#include <cstdint>
#include <memory>
#include <limits>

namespace rnqnx::views {

/// Encoded images on disk, bounded by a byte budget with least-recently-used
/// eviction. One file per key, named by the key's SHA-256; a file's
/// modification time is its last use, so the index is the directory itself and
/// nothing has to be rebuilt after a restart. All members are thread-safe.
class ImageDiskCache {
 public:
  /// \a directory is created on first write or clear. Copies share the cache state
  /// and retain its lifetime; independently constructed caches do not share locks.
  ImageDiskCache(QString directory, std::int64_t budgetBytes);

  /// The process-wide cache under the platform cache location.
  static ImageDiskCache& shared();

  /// The cached bytes for \a key, or empty. A hit refreshes the entry's age.
  /// Entries exceeding maxBytes are removed before allocating their contents.
  QByteArray read(const QString& key, qint64 maxBytes = std::numeric_limits<qint64>::max());
  void remove(const QString& key);
  /// Stores \a bytes, then evicts the oldest entries until the budget holds.
  /// An entry larger than the whole budget is not stored (returns false).
  bool write(const QString& key, const QByteArray& bytes);
  /// A pending write may commit only in the generation where it began.
  bool write(const QString& key, const QByteArray& bytes, std::uint64_t generation);
  /// Reads the admitted generation without waiting for filesystem work.
  std::uint64_t generation() const;
  bool contains(const QString& key) const;
  /// The file that holds \a key (it may not exist).
  QString pathFor(const QString& key) const;
  /// Immediately invalidates older writes, without I/O or the disk mutex.
  /// Pass the returned generation to completeClear on an I/O worker.
  std::uint64_t beginClear();
  /// Completes an admitted clear. A newer read/write may perform its sweep
  /// first; repeating that clear then preserves entries in the new generation.
  /// Failure leaves the sweep pending: reads miss and writes refuse stale data.
  bool completeClear(std::uint64_t generation);
  /// Synchronously admits and completes a clear (compatibility helper).
  void clear();
  std::int64_t usedBytes() const;
  std::int64_t budgetBytes() const;
  void setBudgetBytes(std::int64_t bytes);
  QString directory() const;

 private:
  bool applyPendingClearLocked() const;
  void evictLocked();

  // Copies retain the same cache state for bounded background I/O.
  struct State;
  std::shared_ptr<State> state_;
};

}  // namespace rnqnx::views
