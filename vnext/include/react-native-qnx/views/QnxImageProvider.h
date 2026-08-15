#pragma once

#include <QHash>
#include <QImage>
#include <QQuickImageProvider>
#include <QString>

namespace rnqnx::views {

/// Serves images this platform decoded itself.
///
/// Qt Quick's `Image` fetches `file:` and `http(s):` sources on its own, but not
/// `data:` URIs — a source set to one stays in the Loading state forever. React
/// Native passes data URIs routinely, so those are decoded here and served back
/// through an `image://` URL, which is Qt's supported mechanism for
/// application-provided pixmaps.
class QnxImageProvider final : public QQuickImageProvider {
 public:
  /// The authority in `image://<id>/…`. Registering under this name is what
  /// lets `source` reach this provider.
  static const char* providerId();

  QnxImageProvider();

  /// Decodes \a dataUri and returns the `image://` URL that serves it, or an
  /// empty string when the URI is not a data URI this can decode. Decoding
  /// happens once per distinct URI.
  QString registerDataUri(const QString& dataUri);

  QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize)
      override;

 private:
  QHash<QString, QImage> images_;
};

}  // namespace rnqnx::views
