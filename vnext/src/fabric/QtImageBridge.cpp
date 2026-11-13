#include "react-native-qnx/fabric/QtImageBridge.h"

#include <QQuickItem>

namespace rnqnx::fabric {

// QML Image.status values (Image.Ready, Image.Loading, Image.Error).
constexpr int kImageStatusReady = 1;
constexpr int kImageStatusLoading = 2;
constexpr int kImageStatusError = 3;

QtImageBridge::QtImageBridge(
    QQuickItem* item,
    facebook::react::ImageSource source,
    VoidCallback onLoadStart,
    LoadCallback onLoad,
    VoidCallback onLoadEnd,
    ErrorCallback onError,
    QObject* parent)
    : QObject(parent),
      item_(item),
      source_(std::move(source)),
      onLoadStart_(std::move(onLoadStart)),
      onLoad_(std::move(onLoad)),
      onLoadEnd_(std::move(onLoadEnd)),
      onError_(std::move(onError)) {}

void QtImageBridge::notify() {
  const int status = item_->property("status").toInt();
  if (status == lastStatus_) {
    return;
  }
  lastStatus_ = status;

  if (status == kImageStatusLoading && !startFired_) {
    startFired_ = true;
    onLoadStart_();
  } else if (status == kImageStatusReady && !endFired_) {
    if (!startFired_) {
      startFired_ = true;
      onLoadStart_();
    }
    endFired_ = true;
    onLoad_(source_);
    onLoadEnd_();
  } else if (status == kImageStatusError && !endFired_) {
    if (!startFired_) {
      startFired_ = true;
      onLoadStart_();
    }
    endFired_ = true;
    onLoadEnd_();

    facebook::react::ImageErrorInfo error;
    // QML's Image has no error text property (Qt prints the reason as a QML
    // warning), so report which source failed rather than an empty string.
    QString reason = item_->property("errorString").toString();
    if (reason.isEmpty()) {
      reason = QStringLiteral("Could not load %1")
                   .arg(item_->property("source").toUrl().toString());
    }
    error.error = reason.toStdString();
    onError_(error);
  }
}

}  // namespace rnqnx::fabric
