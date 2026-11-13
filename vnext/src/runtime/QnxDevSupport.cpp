#include "react-native-qnx/runtime/QnxDevSupport.h"

#include <QElapsedTimer>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QUrlQuery>
#include <QWebSocket>

#include <cstdio>

namespace rnqnx::runtime {
namespace {

// The developer overlay: loading banner (DevLoadingView), the native error
// screen and the developer menu. One QML component so the three share a
// look; C++ drives it through properties and listens to `action`.
constexpr const char* kOverlaySource = R"QML(
import QtQuick

Item {
  id: dev
  z: 3000000

  property string message: ""
  property color messageColor: "#ffffff"
  property color messageBackground: "#2584e8"
  property bool messageVisible: false

  property bool errorVisible: false
  property string errorTitle: ""
  property string errorText: ""

  property bool menuVisible: false
  property var menuItems: []
  // Keyboard selection in the menu: 0 Reload, 1 Fast Refresh, then the
  // app's items, then Close.
  property int menuIndex: 0
  property bool hotLoading: true
  property string serverText: ""
  property bool packagerConnected: false

  signal action(string name)

  readonly property string sans: "DejaVu Sans"
  readonly property string mono: "DejaVu Sans Mono"

  // Loading / refreshing banner, top centre like the iOS and Android ones.
  Rectangle {
    id: banner
    anchors.horizontalCenter: parent.horizontalCenter
    y: dev.messageVisible ? 12 : -height - 4
    height: 34
    width: Math.min(parent.width - 24, bannerText.implicitWidth + 36)
    radius: 17
    color: dev.messageBackground
    opacity: dev.messageVisible ? 0.96 : 0
    visible: opacity > 0
    Behavior on y { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
    Behavior on opacity { NumberAnimation { duration: 160 } }
    Text {
      id: bannerText
      anchors.centerIn: parent
      width: Math.min(implicitWidth, parent.width - 24)
      elide: Text.ElideRight
      text: dev.message
      color: dev.messageColor
      font.family: dev.sans
      font.pixelSize: 14
    }
  }

  // Shield: present while the error screen or the menu is up, so presses do
  // not fall through to React views.
  Item {
    objectName: "rnqnxDevShield"
    anchors.fill: parent
    visible: dev.errorVisible || dev.menuVisible
    MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons; onWheel: (w) => w.accepted = true }
  }

  component DevButton: Rectangle {
    id: button
    property string label: ""
    property string hint: ""
    property color fill: "#2b3a4f"
    property color ink: "#f3f6fa"
    property bool highlighted: false
    signal clicked()
    implicitHeight: 42
    radius: 9
    border.width: highlighted ? 2 : 0
    border.color: "#79e8dc"
    color: area.pressed ? Qt.darker(fill, 1.25) : (area.containsMouse || highlighted) ? Qt.lighter(fill, 1.18) : fill
    Text {
      anchors.left: parent.left; anchors.leftMargin: 16
      anchors.verticalCenter: parent.verticalCenter
      text: button.label; color: button.ink
      font.family: dev.sans; font.pixelSize: 15
    }
    Text {
      anchors.right: parent.right; anchors.rightMargin: 16
      anchors.verticalCenter: parent.verticalCenter
      text: button.hint; color: Qt.rgba(1, 1, 1, 0.55)
      font.family: dev.sans; font.pixelSize: 12
    }
    MouseArea {
      id: area
      anchors.fill: parent
      hoverEnabled: true
      cursorShape: Qt.PointingHandCursor
      onClicked: button.clicked()
    }
  }

  // Native error screen: bundle could not be fetched or evaluated.
  Rectangle {
    anchors.fill: parent
    visible: dev.errorVisible
    color: "#7f1d1d"
    Column {
      id: errorHead
      anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
      anchors.margins: 28
      spacing: 6
      Text {
        text: "DEVELOPMENT  ·  " + dev.serverText
        color: Qt.rgba(1, 1, 1, 0.65)
        font.family: dev.sans; font.pixelSize: 12; font.letterSpacing: 1
      }
      Text {
        width: parent.width
        text: dev.errorTitle
        color: "#ffffff"
        wrapMode: Text.Wrap
        font.family: dev.sans; font.pixelSize: 24; font.bold: true
      }
    }
    Rectangle {
      anchors.left: parent.left; anchors.right: parent.right
      anchors.top: errorHead.bottom; anchors.bottom: errorButtons.top
      anchors.margins: 28
      radius: 10
      color: Qt.rgba(0, 0, 0, 0.28)
      clip: true
      Flickable {
        id: errorFlick
        anchors.fill: parent
        anchors.margins: 16
        contentWidth: width
        contentHeight: errorBody.implicitHeight
        boundsBehavior: Flickable.StopAtBounds
        Text {
          id: errorBody
          width: errorFlick.width
          text: dev.errorText
          color: "#fde8e8"
          wrapMode: Text.WrapAnywhere
          textFormat: Text.PlainText
          font.family: dev.mono; font.pixelSize: 14
          lineHeight: 1.2
        }
      }
    }
    Row {
      id: errorButtons
      anchors.left: parent.left; anchors.bottom: parent.bottom
      anchors.margins: 28
      spacing: 12
      DevButton { width: 220; label: "Reload"; hint: "Ctrl+R"; fill: "#b91c1c"; onClicked: dev.action("reload") }
      DevButton { width: 160; label: "Dismiss"; hint: "Esc"; fill: "#3f1d1d"; onClicked: dev.action("dismissError") }
    }
  }

  // Developer menu.
  Rectangle {
    anchors.fill: parent
    visible: dev.menuVisible
    color: Qt.rgba(0, 0, 0, 0.55)
    MouseArea { anchors.fill: parent; onClicked: dev.action("closeMenu") }
    Rectangle {
      anchors.centerIn: parent
      width: Math.min(parent.width - 32, 420)
      height: menuColumn.implicitHeight + 40
      radius: 16
      color: "#111c2b"
      border.color: Qt.rgba(1, 1, 1, 0.08)
      MouseArea { anchors.fill: parent }
      Column {
        id: menuColumn
        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
        anchors.margins: 20
        spacing: 8
        Text {
          text: "React Native QNX"
          color: "#f3f6fa"; font.family: dev.sans; font.pixelSize: 20; font.bold: true
        }
        Text {
          width: parent.width
          text: (dev.packagerConnected ? "Connected to Metro at " : "Metro not connected · ") + dev.serverText
          color: dev.packagerConnected ? "#79e8dc" : "#fca5a5"
          elide: Text.ElideRight
          font.family: dev.sans; font.pixelSize: 13
        }
        Item { width: 1; height: 6 }
        DevButton { width: parent.width; label: "Reload"; hint: "Ctrl+R"; highlighted: dev.menuIndex === 0; onClicked: dev.action("reload") }
        DevButton {
          width: parent.width
          highlighted: dev.menuIndex === 1
          label: dev.hotLoading ? "Disable Fast Refresh" : "Enable Fast Refresh"
          hint: dev.hotLoading ? "on" : "off"
          onClicked: dev.action("toggleHot")
        }
        Repeater {
          model: dev.menuItems
          DevButton {
            required property string modelData
            required property int index
            width: menuColumn.width
            label: modelData
            highlighted: dev.menuIndex === index + 2
            onClicked: dev.action("item:" + modelData)
          }
        }
        DevButton { width: parent.width; label: "Close"; hint: "Esc"; fill: "#1d2a3c"; highlighted: dev.menuIndex === dev.menuItems.length + 2; onClicked: dev.action("closeMenu") }
        Text {
          width: parent.width
          horizontalAlignment: Text.AlignHCenter
          text: "↑ ↓ to choose · Enter to run"
          color: Qt.rgba(1, 1, 1, 0.45)
          font.family: dev.sans; font.pixelSize: 12
        }
      }
    }
  }
}
)QML";

QString stripAnsi(QString text) {
  static const QRegularExpression ansi(QStringLiteral("\\x1B\\[[0-9;]*[A-Za-z]"));
  return text.remove(ansi);
}

QColor colorFrom(std::optional<std::uint32_t> argb, const QColor& fallback) {
  if (!argb) return fallback;
  return QColor::fromRgba(*argb);
}

/// Metro answers a failed build with JSON: {type, message, errors: [...]}.
/// Turn it into a title and readable text.
void describeMetroError(const QByteArray& body, int status, DevServerBundle& out) {
  const QJsonDocument document = QJsonDocument::fromJson(body);
  if (document.isObject()) {
    const QJsonObject error = document.object();
    const QString type = error.value(QStringLiteral("type")).toString();
    out.errorTitle = type.isEmpty() ? QStringLiteral("Metro could not build the bundle")
                                    : type;
    QString message = error.value(QStringLiteral("message")).toString();
    if (message.isEmpty()) {
      for (const QJsonValue& item : error.value(QStringLiteral("errors")).toArray()) {
        message += item.toObject().value(QStringLiteral("description")).toString() + '\n';
      }
    }
    out.errorMessage = stripAnsi(message).trimmed();
    return;
  }
  out.errorTitle = QStringLiteral("Metro answered HTTP %1").arg(status);
  out.errorMessage = stripAnsi(QString::fromUtf8(body.left(8192))).trimmed();
}

}  // namespace

/// Forwards the TurboModules' calls to the Qt loop. The modules call it inside
/// a JavaScript task; queuing keeps reload and surface changes out of that
/// task, and QPointer makes a call after the client is gone a no-op.
class QnxDevSupport::Delegate final : public DevSupportDelegate {
 public:
  explicit Delegate(QnxDevSupport* owner) : owner_(owner) {}

  void reload(const std::string& reason) override {
    post([reason = QString::fromStdString(reason)](QnxDevSupport& d) {
      d.requestReload(reason.isEmpty() ? QStringLiteral("DevSettings") : reason);
    });
  }
  void setHotLoadingEnabled(bool enabled) override {
    post([enabled](QnxDevSupport& d) {
      d.hotLoading_ = enabled;
      if (d.overlay_) d.overlay_->setProperty("hotLoading", enabled);
    });
  }
  void addMenuItem(const std::string& title) override {
    post([title = QString::fromStdString(title)](QnxDevSupport& d) { d.addMenuItem(title); });
  }
  void showMessage(const std::string& message, std::optional<std::uint32_t> text,
                   std::optional<std::uint32_t> background) override {
    post([message = QString::fromStdString(message), text, background](QnxDevSupport& d) {
      d.showMessage(message, colorFrom(text, QColor(Qt::white)),
                    colorFrom(background, QColor(0x25, 0x84, 0xe8)));
    });
  }
  void hideMessage() override {
    post([](QnxDevSupport& d) { d.hideMessage(); });
  }
  void showLogBox() override {
    post([](QnxDevSupport& d) { Q_EMIT d.showLogBoxRequested(); });
  }
  void hideLogBox() override {
    post([](QnxDevSupport& d) { Q_EMIT d.hideLogBoxRequested(); });
  }

 private:
  template <typename F>
  void post(F work) {
    QPointer<QnxDevSupport> owner = owner_;
    if (!owner) return;
    QMetaObject::invokeMethod(owner, [owner, work = std::move(work)]() {
      if (owner) work(*owner);
    }, Qt::QueuedConnection);
  }

  QPointer<QnxDevSupport> owner_;
};

std::optional<QUrl> QnxDevSupport::parseServer(const QString& text, QString* error) {
  QUrl url = QUrl::fromUserInput(text.trimmed());
  if (!text.contains(QStringLiteral("://"))) url = QUrl(QStringLiteral("http://") + text.trimmed());
  const auto fail = [error](const QString& why) -> std::optional<QUrl> {
    if (error) *error = why;
    return std::nullopt;
  };
  if (!url.isValid() || url.host().isEmpty()) return fail(QStringLiteral("not a URL"));
  if (url.scheme() != QLatin1String("http") && url.scheme() != QLatin1String("https"))
    return fail(QStringLiteral("use http:// or https://"));
  if (!url.userInfo().isEmpty() || url.hasQuery() || url.hasFragment() ||
      (!url.path().isEmpty() && url.path() != QLatin1String("/")))
    return fail(QStringLiteral("give only scheme, host and port"));
  if (url.port() == -1) url.setPort(8081);
  url.setPath(QString());
  return url;
}

QnxDevSupport::QnxDevSupport(Options options, QQuickWindow& window, QQmlEngine& engine,
                             QQuickItem& surfaceRoot)
    : options_(std::move(options)),
      window_(window),
      surfaceRoot_(surfaceRoot),
      network_(new QNetworkAccessManager(this)),
      delegate_(std::make_shared<Delegate>(this)) {
  QQuickItem* parent = surfaceRoot_.parentItem() ? surfaceRoot_.parentItem() : &surfaceRoot_;

  // The LogBox surface's root: a sibling over the app's surface root, so the
  // app's own child indices never see it.
  logBoxRoot_ = new QQuickItem(parent);
  logBoxRoot_->setObjectName(QStringLiteral("rnqnxLogBoxRoot"));
  logBoxRoot_->setZ(2000000);
  logBoxRoot_->setClip(true);
  logBoxRoot_->setVisible(false);

  QQmlComponent component(&engine);
  component.setData(QByteArray(kOverlaySource), QUrl(QStringLiteral("qrc:/rnqnx/DevOverlay.qml")));
  QObject* created = component.create();
  overlay_ = qobject_cast<QQuickItem*>(created);
  if (!overlay_) {
    delete created;
    std::fprintf(stderr, "RNQNX_DEV_OVERLAY_ERROR %s\n",
                 component.errorString().toUtf8().constData());
  } else {
    overlay_->setParentItem(parent);
    overlay_->setParent(this);
    overlay_->setProperty("serverText", options_.server.toString());
    shield_ = overlay_->findChild<QQuickItem*>(QStringLiteral("rnqnxDevShield"));
    connect(overlay_, SIGNAL(action(QString)), this, SLOT(onOverlayAction(QString)));
  }
  if (!shield_) {
    shield_ = new QQuickItem(parent);
    shield_->setVisible(false);
  }

  for (auto signal : {&QQuickItem::xChanged, &QQuickItem::yChanged,
                      &QQuickItem::widthChanged, &QQuickItem::heightChanged}) {
    connect(&surfaceRoot_, signal, this, &QnxDevSupport::syncGeometry);
  }
  syncGeometry();
  window_.installEventFilter(this);

  packagerRetry_.setSingleShot(true);
  packagerRetry_.setInterval(3000);
  connect(&packagerRetry_, &QTimer::timeout, this, &QnxDevSupport::connectPackager);
  connectPackager();
  std::printf("RNQNX_DEV_SERVER %s\n", options_.server.toString().toUtf8().constData());
  std::fflush(stdout);
}

QnxDevSupport::~QnxDevSupport() {
  window_.removeEventFilter(this);
  if (packager_) {
    packager_->disconnect(this);
    packager_->abort();
  }
  // The overlay and LogBox root are borrowed children of the window's scene;
  // delete them before the engine and window go.
  delete overlay_;
  delete logBoxRoot_;
}

void QnxDevSupport::syncGeometry() {
  const auto place = [this](QQuickItem* item) {
    if (!item || item == &surfaceRoot_) return;
    if (item->parentItem() == &surfaceRoot_) {
      item->setPosition(QPointF(0, 0));
    } else {
      item->setPosition(surfaceRoot_.position());
    }
    item->setSize(surfaceRoot_.size());
  };
  place(logBoxRoot_);
  place(overlay_);
}

QUrl QnxDevSupport::bundleUrl() const {
  QUrl url = options_.server;
  url.setPath(QLatin1Char('/') + options_.entry + QStringLiteral(".bundle"));
  QUrlQuery query;
  query.addQueryItem(QStringLiteral("platform"), options_.platform);
  query.addQueryItem(QStringLiteral("dev"), QStringLiteral("true"));
  query.addQueryItem(QStringLiteral("lazy"), QStringLiteral("false"));
  query.addQueryItem(QStringLiteral("minify"), QStringLiteral("false"));
  query.addQueryItem(QStringLiteral("modulesOnly"), QStringLiteral("false"));
  query.addQueryItem(QStringLiteral("runModule"), QStringLiteral("true"));
  if (!options_.appId.isEmpty()) query.addQueryItem(QStringLiteral("app"), options_.appId);
  url.setQuery(query);
  return url;
}

QString QnxDevSupport::bundleEntry() const { return options_.entry + QStringLiteral(".bundle"); }
QString QnxDevSupport::host() const { return options_.server.host(); }
int QnxDevSupport::port() const { return options_.server.port(8081); }
QString QnxDevSupport::scheme() const { return options_.server.scheme(); }
QString QnxDevSupport::platform() const { return options_.platform; }
bool QnxDevSupport::hotLoadingEnabled() const { return hotLoading_; }
bool QnxDevSupport::packagerConnected() const {
  return packager_ && packager_->state() == QAbstractSocket::ConnectedState;
}

std::shared_ptr<DevSupportDelegate> QnxDevSupport::delegate() const { return delegate_; }
QQuickItem& QnxDevSupport::logBoxRoot() { return *logBoxRoot_; }
QQuickItem& QnxDevSupport::inputShield() { return *shield_; }

DevServerBundle QnxDevSupport::fetchBundle(int timeoutMs) {
  DevServerBundle result;
  const QUrl url = bundleUrl();
  result.url = url.toString(QUrl::FullyEncoded);
  const QString where = options_.server.authority();
  showMessage(QStringLiteral("Loading %1 from %2…").arg(bundleEntry(), where),
              QColor(Qt::white), QColor(0x40, 0x40, 0x40));

  QNetworkRequest request(url);
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                       QNetworkRequest::ManualRedirectPolicy);
  request.setRawHeader("Accept", "application/javascript, application/json");
  request.setTransferTimeout(timeoutMs);
  QElapsedTimer elapsed;
  elapsed.start();
  QNetworkReply* reply = network_->get(request);
  QEventLoop loop;
  QTimer ticker;
  ticker.setInterval(1000);
  connect(&ticker, &QTimer::timeout, this, [&]() {
    showMessage(QStringLiteral("Loading %1 from %2… %3 s")
                    .arg(bundleEntry(), where)
                    .arg(elapsed.elapsed() / 1000),
                QColor(Qt::white), QColor(0x40, 0x40, 0x40));
  });
  ticker.start();
  connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
  loop.exec();
  ticker.stop();

  const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  const QByteArray body = reply->readAll();
  waitingForServer_ = false;
  if (reply->error() == QNetworkReply::NoError && status == 200) {
    result.ok = true;
    result.source = body;
    std::printf("RNQNX_DEV_BUNDLE_LOADED bytes=%lld ms=%lld url=%s\n",
                static_cast<long long>(body.size()), static_cast<long long>(elapsed.elapsed()),
                result.url.toUtf8().constData());
  } else if (status >= 400) {
    describeMetroError(body, status, result);
  } else {
    waitingForServer_ = true;
    result.errorTitle = QStringLiteral("Could not connect to Metro");
    result.errorMessage =
        QStringLiteral("%1\n\nURL: %2\n\nStart the development server on the computer this board "
                       "is connected to:\n\n  rnqnx project start --project <app>\n\n"
                       "or run `rnqnx project dev` to start it and launch this app in one step. "
                       "The app loads as soon as Metro answers; Reload tries again now.")
            .arg(reply->errorString(), result.url);
  }
  if (!result.ok) {
    std::printf("RNQNX_DEV_BUNDLE_FAILED %s: %s\n", result.errorTitle.toUtf8().constData(),
                result.errorMessage.left(400).toUtf8().constData());
  }
  std::fflush(stdout);
  reply->deleteLater();
  hideMessage();
  return result;
}

void QnxDevSupport::showMessage(const QString& message, const QColor& text,
                                const QColor& background) {
  if (!overlay_) return;
  overlay_->setProperty("message", message);
  overlay_->setProperty("messageColor", text);
  overlay_->setProperty("messageBackground", background);
  overlay_->setProperty("messageVisible", true);
}

void QnxDevSupport::hideMessage() {
  if (overlay_) overlay_->setProperty("messageVisible", false);
}

void QnxDevSupport::showError(const QString& title, const QString& message) {
  std::printf("RNQNX_DEV_ERROR %s\n", title.toUtf8().constData());
  std::fflush(stdout);
  if (!overlay_) return;
  overlay_->setProperty("errorTitle", title);
  overlay_->setProperty("errorText", message);
  overlay_->setProperty("menuVisible", false);
  overlay_->setProperty("errorVisible", true);
}

void QnxDevSupport::hideError() {
  waitingForServer_ = false;
  if (overlay_) overlay_->setProperty("errorVisible", false);
}

void QnxDevSupport::setMenuVisible(bool visible) {
  if (!overlay_) return;
  if (visible) {
    overlay_->setProperty("packagerConnected", packagerConnected());
    overlay_->setProperty("menuIndex", 0);
  }
  overlay_->setProperty("menuVisible", visible);
  std::printf("RNQNX_DEV_MENU %s\n", visible ? "open" : "closed");
  std::fflush(stdout);
}

bool QnxDevSupport::menuVisible() const {
  return overlay_ && overlay_->property("menuVisible").toBool();
}

void QnxDevSupport::addMenuItem(const QString& title) {
  if (title.isEmpty() || menuItems_.contains(title)) return;
  menuItems_.append(title);
  if (overlay_) overlay_->setProperty("menuItems", menuItems_);
}

void QnxDevSupport::resetForReload() {
  reloadPending_ = false;
  waitingForServer_ = false;
  menuItems_.clear();
  if (overlay_) {
    overlay_->setProperty("menuItems", menuItems_);
    overlay_->setProperty("menuVisible", false);
    overlay_->setProperty("errorVisible", false);
    overlay_->setProperty("messageVisible", false);
  }
  logBoxRoot_->setVisible(false);
}

void QnxDevSupport::requestReload(const QString& reason) {
  // Several sources can ask at once (Metro `r` plus a key press); one reload.
  if (reloadPending_) return;
  reloadPending_ = true;
  std::printf("RNQNX_DEV_RELOAD %s\n", reason.toUtf8().constData());
  std::fflush(stdout);
  if (overlay_) {
    overlay_->setProperty("menuVisible", false);
    overlay_->setProperty("errorVisible", false);
  }
  showMessage(QStringLiteral("Reloading…"), QColor(Qt::white), QColor(0x40, 0x40, 0x40));
  Q_EMIT reloadRequested(reason);
}

void QnxDevSupport::setHotLoading(bool enabled) {
  hotLoading_ = enabled;
  if (overlay_) overlay_->setProperty("hotLoading", enabled);
  Q_EMIT hotLoadingToggled(enabled);
}

void QnxDevSupport::onOverlayAction(const QString& action) {
  if (action == QLatin1String("reload")) {
    requestReload(QStringLiteral("menu"));
  } else if (action == QLatin1String("dismissError")) {
    hideError();
  } else if (action == QLatin1String("closeMenu")) {
    setMenuVisible(false);
  } else if (action == QLatin1String("toggleHot")) {
    setMenuVisible(false);
    setHotLoading(!hotLoading_);
    std::printf("RNQNX_DEV_FAST_REFRESH %s\n", hotLoading_ ? "on" : "off");
    std::fflush(stdout);
  } else if (action.startsWith(QLatin1String("item:"))) {
    setMenuVisible(false);
    std::printf("RNQNX_DEV_MENU_ITEM %s\n", action.mid(5).toUtf8().constData());
    std::fflush(stdout);
    Q_EMIT menuItemSelected(action.mid(5));
  }
}

bool QnxDevSupport::eventFilter(QObject* watched, QEvent* event) {
  if (watched == &window_ && event->type() == QEvent::KeyPress) {
    auto* key = static_cast<QKeyEvent*>(event);
    const bool ctrl = key->modifiers().testFlag(Qt::ControlModifier);
    if ((ctrl && key->key() == Qt::Key_R) || key->key() == Qt::Key_F5) {
      requestReload(QStringLiteral("keyboard"));
      return true;
    }
    if ((ctrl && key->key() == Qt::Key_D) || key->key() == Qt::Key_F1) {
      setMenuVisible(!menuVisible());
      return true;
    }
    if (menuVisible()) {
      const int last = static_cast<int>(menuItems_.size()) + 2;
      int index = overlay_->property("menuIndex").toInt();
      if (key->key() == Qt::Key_Down || key->key() == Qt::Key_Up) {
        index = (index + (key->key() == Qt::Key_Down ? 1 : last)) % (last + 1);
        overlay_->setProperty("menuIndex", index);
        return true;
      }
      if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
        if (index == 0) onOverlayAction(QStringLiteral("reload"));
        else if (index == 1) onOverlayAction(QStringLiteral("toggleHot"));
        else if (index < last) onOverlayAction(QStringLiteral("item:") + menuItems_.at(index - 2));
        else onOverlayAction(QStringLiteral("closeMenu"));
        return true;
      }
    }
    if (key->key() == Qt::Key_Escape && overlay_ &&
        (overlay_->property("menuVisible").toBool() || overlay_->property("errorVisible").toBool())) {
      setMenuVisible(false);
      hideError();
      return true;
    }
  }
  return QObject::eventFilter(watched, event);
}

void QnxDevSupport::connectPackager() {
  // Metro's /message socket carries `r` (reload) and `d` (dev menu) from the
  // terminal: {"version": 2, "method": "reload"}.
  if (!packager_) {
    packager_ = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
    connect(packager_, &QWebSocket::textMessageReceived, this, [this](const QString& text) {
      const QJsonObject message = QJsonDocument::fromJson(text.toUtf8()).object();
      const QString method = message.value(QStringLiteral("method")).toString();
      if (method == QLatin1String("reload")) {
        requestReload(QStringLiteral("Metro"));
      } else if (method == QLatin1String("devMenu")) {
        setMenuVisible(!menuVisible());
      }
    });
    connect(packager_, &QWebSocket::connected, this, [this]() {
      std::printf("RNQNX_DEV_PACKAGER connected\n");
      std::fflush(stdout);
      if (overlay_) overlay_->setProperty("packagerConnected", true);
      // Metro came up after the app showed "Could not connect", or Metro was
      // restarted (its Fast Refresh graph is gone): load the bundle again
      // instead of waiting for someone to press Reload.
      if (waitingForServer_ || packagerLost_) {
        packagerLost_ = false;
        requestReload(QStringLiteral("Metro connected"));
      }
    });
    connect(packager_, &QWebSocket::disconnected, this, [this]() {
      if (overlay_ && overlay_->property("packagerConnected").toBool()) packagerLost_ = true;
      if (overlay_) overlay_->setProperty("packagerConnected", false);
      packagerRetry_.start();
    });
    connect(packager_, &QWebSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
      if (packager_->state() == QAbstractSocket::UnconnectedState) packagerRetry_.start();
    });
  }
  QUrl url = options_.server;
  url.setScheme(options_.server.scheme() == QLatin1String("https") ? QStringLiteral("wss")
                                                                   : QStringLiteral("ws"));
  url.setPath(QStringLiteral("/message"));
  QUrlQuery query;
  query.addQueryItem(QStringLiteral("role"), QStringLiteral("client"));
  url.setQuery(query);
  packager_->open(url);
}

}  // namespace rnqnx::runtime
