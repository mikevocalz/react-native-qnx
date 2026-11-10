#pragma once

#include "react-native-qnx/runtime/QtReactHost.h"
#include <QByteArray>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QTimer>
#include <QUrl>
#include <cstdint>
#include <functional>
#include <map>

class QNetworkReply;

namespace rnqnx::runtime {

// Owner-thread lifecycle for the existing Fabric host. The window, mounting
// layer, adapter and input dispatcher outlive the session and every reload.
class DevSession final : public QObject {
 public:
  struct Configuration {
    QUrl bundleUrl;
    std::string componentName;
    facebook::react::SurfaceId surfaceId{1};
    int fetchTimeoutMs{15000};
    qint64 maxBundleBytes{32 * 1024 * 1024};
    int maxFetchAttempts{3};
  };
  enum class Phase { Stopped, Fetching, Loading, Ready, Disconnected, Error };
  struct Status { Phase phase; std::uint64_t generation; QString message; };
  struct Subscription { std::function<void()> remove; };
  using PrepareHost = std::function<void(QtReactHost&)>;

  DevSession(fabric::ShadowViewMutationAdapter& adapter,
      Configuration configuration, PrepareHost prepareHost = {});
  ~DevSession() override;
  void start();
  void requestReload(const QString& reason);
  void stop();
  void resize(float width, float height);
  Status status() const;
  // Initial snapshot is delivered synchronously; later deliveries are on owner.
  Subscription addStatusListener(std::function<void(const Status&)> listener);
  static QString validate(const Configuration& configuration);

 private:
  void beginGeneration();
  void fetch();
  void fetchFailed(const QString& message);
  void cancelFetch();
  void replaceHost();
  void report(std::uint64_t generation, const std::string& kind,
              const std::string& message);
  void startSurfaceIfReady();
  void publish(Phase phase, QString message);
  void requireOwner() const;

  fabric::ShadowViewMutationAdapter& adapter_;
  const Configuration configuration_;
  PrepareHost prepareHost_;
  QNetworkAccessManager network_;
  QPointer<QNetworkReply> reply_;
  QTimer fetchDeadline_;
  QTimer fetchRetry_;
  QByteArray source_;
  std::unique_ptr<QtReactHost> host_;
  Status status_{Phase::Stopped, 0, {}};
  std::map<std::uint64_t, std::function<void(const Status&)>> listeners_;
  std::uint64_t nextListener_{0};
  int fetchAttempts_{0};
  float width_{0};
  float height_{0};
  bool surfaceStarted_{false};
  bool bundleReady_{false};
  bool reloadPending_{false};
  bool graphReloadUsed_{false};
};
} // namespace rnqnx::runtime
