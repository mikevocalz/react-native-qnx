#include "WindowChromeModel.h"

#include <QJsonValue>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace rnqnx::window {
namespace {

[[noreturn]] void reject(const QString& message) {
  throw std::invalid_argument(message.toStdString());
}

int edge(const QJsonObject& size, const QString& key) {
  const QJsonValue value = size.value(key);
  const double number = value.toDouble(-1);
  if (!value.isDouble() || std::floor(number) != number || number < 1 || number > kMaxWindowEdge) {
    reject(QStringLiteral("window.initialSize.%1 must be an integer from 1 to %2.")
               .arg(key)
               .arg(kMaxWindowEdge));
  }
  return static_cast<int>(number);
}

QString accessibleName(WindowAction action) {
  switch (action) {
    case WindowAction::Close:
      return QStringLiteral("Close window");
    case WindowAction::Minimize:
      return QStringLiteral("Minimize window");
    case WindowAction::Maximize:
      return QStringLiteral("Maximize window");
  }
  return {};
}

const QString kQnxMinimizeReason = QStringLiteral(
    "Qt 6.12.0 QQnxWindow::minimize() only logs that minimize is unsupported, and no "
    "window-manager client on the image can restore the window (ADR-0016)");
const QString kQnxMaximizeReason = QStringLiteral(
    "the qnx QPA keeps the root window at screen size unless it runs with no-fullscreen");
const QString kNoWindowManagerReason =
    QStringLiteral("this QPA has no window manager to restore a minimized window");
const QString kUnknownReason = QStringLiteral("not verified on this QPA");
const QString kWaylandReason = QStringLiteral(
    "not verified on a Wayland compositor; the Marathon shell may own window placement and "
    "restore");
const QString kSurfaceReason = QStringLiteral(
    "the React surface keeps its first size, so a maximized window would not resize the app");

}  // namespace

ControlsLayout parseControlsLayout(const QString& value) {
  if (value == QLatin1String("macos")) return ControlsLayout::MacOS;
  if (value == QLatin1String("windows")) return ControlsLayout::Windows;
  reject(QStringLiteral("window.controls must be one of: macos, windows."));
}

TitleBarStyle parseTitleBarStyle(const QString& value) {
  if (value == QLatin1String("default")) return TitleBarStyle::Default;
  if (value == QLatin1String("hidden")) return TitleBarStyle::Hidden;
  if (value == QLatin1String("none")) return TitleBarStyle::None;
  reject(QStringLiteral("window.titleBarStyle must be one of: default, hidden, none."));
}

UnavailableControls parseUnavailableControls(const QString& value) {
  if (value == QLatin1String("hide")) return UnavailableControls::Hide;
  if (value == QLatin1String("disable")) return UnavailableControls::Disable;
  reject(QStringLiteral("unavailable controls must be one of: hide, disable."));
}

QString nameOf(ControlsLayout value) {
  return value == ControlsLayout::MacOS ? QStringLiteral("macos") : QStringLiteral("windows");
}

QString nameOf(TitleBarStyle value) {
  switch (value) {
    case TitleBarStyle::Default:
      return QStringLiteral("default");
    case TitleBarStyle::Hidden:
      return QStringLiteral("hidden");
    case TitleBarStyle::None:
      return QStringLiteral("none");
  }
  return {};
}

QString nameOf(WindowAction value) {
  switch (value) {
    case WindowAction::Close:
      return QStringLiteral("close");
    case WindowAction::Minimize:
      return QStringLiteral("minimize");
    case WindowAction::Maximize:
      return QStringLiteral("maximize");
  }
  return {};
}

WindowChromeConfig parseWindowChromeConfig(const QJsonObject& window) {
  for (auto it = window.begin(); it != window.end(); ++it) {
    if (it.key() != QLatin1String("controls") && it.key() != QLatin1String("titleBarStyle") &&
        it.key() != QLatin1String("initialSize")) {
      reject(QStringLiteral("window.%1 is not a supported window option.").arg(it.key()));
    }
  }
  WindowChromeConfig config;
  if (window.contains(QLatin1String("controls"))) {
    const QJsonValue value = window.value(QLatin1String("controls"));
    if (!value.isString()) reject(QStringLiteral("window.controls must be one of: macos, windows."));
    config.controls = parseControlsLayout(value.toString());
  }
  if (window.contains(QLatin1String("titleBarStyle"))) {
    const QJsonValue value = window.value(QLatin1String("titleBarStyle"));
    if (!value.isString())
      reject(QStringLiteral("window.titleBarStyle must be one of: default, hidden, none."));
    config.titleBarStyle = parseTitleBarStyle(value.toString());
  }
  if (window.contains(QLatin1String("initialSize"))) {
    const QJsonValue value = window.value(QLatin1String("initialSize"));
    if (!value.isObject()) reject(QStringLiteral("window.initialSize must be {width, height}."));
    const QJsonObject size = value.toObject();
    if (size.size() != 2 || !size.contains(QLatin1String("width")) ||
        !size.contains(QLatin1String("height"))) {
      reject(QStringLiteral("window.initialSize accepts only width and height."));
    }
    config.initialSize = QSize(edge(size, QStringLiteral("width")), edge(size, QStringLiteral("height")));
  }
  return config;
}

WindowCapabilities capabilitiesForPlatform(const QString& platform,
                                           const QStringList& platformOptions,
                                           SurfaceResize surface) {
  WindowCapabilities caps;
  caps.platform = platform;
  const ActionSupport live{true, {}};
  if (platform == QLatin1String("qnx")) {
    caps.minimize = {false, kQnxMinimizeReason};
    caps.maximize = platformOptions.contains(QStringLiteral("no-fullscreen"))
                        ? live
                        : ActionSupport{false, kQnxMaximizeReason};
    caps.systemMove = false;  // QQnxWindow does not override startSystemMove().
  } else if (platform == QLatin1String("cocoa") || platform == QLatin1String("windows") ||
             platform == QLatin1String("xcb")) {
    caps.minimize = live;
    caps.maximize = live;
    caps.systemMove = true;  // startSystemMove() still reports failure at runtime.
  } else if (platform == QLatin1String("offscreen") || platform == QLatin1String("minimal")) {
    caps.minimize = {false, kNoWindowManagerReason};
    caps.maximize = live;
    caps.systemMove = false;
  } else if (platform.startsWith(QLatin1String("wayland"))) {
    caps.minimize = {false, kWaylandReason};
    caps.maximize = {false, kWaylandReason};
    caps.systemMove = false;
  } else {
    caps.minimize = {false, kUnknownReason};
    caps.maximize = {false, kUnknownReason};
    caps.systemMove = false;
  }
  if (caps.maximize.available && surface == SurfaceResize::Fixed) caps.maximize = {false, kSurfaceReason};
  return caps;
}

bool platformForcesFullScreen(const QString& platform, const QStringList& platformOptions) {
  return platform == QLatin1String("qnx") && !platformOptions.contains(QStringLiteral("no-fullscreen"));
}

QStringList platformSpecsFromArguments(int argc, const char* const* argv,
                                       const QString& environmentSpec) {
  QStringList specs = environmentSpec.split(QLatin1Char(';'), Qt::SkipEmptyParts);
  for (int i = 1; i < argc; ++i) {
    const QString argument = QString::fromLocal8Bit(argv[i]);
    if (argument == QLatin1String("-platform") || argument == QLatin1String("--platform")) {
      specs = i + 1 < argc ? QStringList{QString::fromLocal8Bit(argv[i + 1])} : QStringList{};
      ++i;
    } else if (argument.startsWith(QLatin1String("-platform=")) ||
               argument.startsWith(QLatin1String("--platform="))) {
      specs = QStringList{argument.mid(argument.indexOf(QLatin1Char('=')) + 1)};
    }
  }
  return specs;
}

QStringList platformOptionsFor(const QString& platform, const QStringList& specs) {
  for (const QString& spec : specs) {
    QStringList parts = spec.split(QLatin1Char(':'), Qt::SkipEmptyParts);
    if (parts.isEmpty() || parts.first() != platform) continue;
    parts.removeFirst();
    return parts;
  }
  return {};
}

ChromeLayout layoutChrome(const WindowChromeConfig& config,
                          const WindowCapabilities& capabilities,
                          qreal windowWidth) {
  ChromeLayout layout;
  if (config.titleBarStyle == TitleBarStyle::None) return layout;

  const bool mac = config.controls == ControlsLayout::MacOS;
  const std::vector<WindowAction> order =
      mac ? std::vector<WindowAction>{WindowAction::Close, WindowAction::Minimize, WindowAction::Maximize}
          : std::vector<WindowAction>{WindowAction::Minimize, WindowAction::Maximize, WindowAction::Close};
  std::vector<std::pair<WindowAction, ActionSupport>> shown;
  for (const auto action : order) {
    const ActionSupport support = action == WindowAction::Close      ? ActionSupport{true, {}}
                                  : action == WindowAction::Minimize ? capabilities.minimize
                                                                     : capabilities.maximize;
    if (support.available) {
      shown.emplace_back(action, support);
    } else if (config.unavailableControls == UnavailableControls::Disable) {
      shown.emplace_back(action, support);
      layout.disabled.push_back({action, support.reason});
    } else {
      layout.hidden.push_back({action, support.reason});
    }
  }

  const qreal hitWidth = mac ? kMacHitWidth : kWindowsHitWidth;
  const qreal rowWidth = hitWidth * static_cast<qreal>(shown.size());
  qreal x = mac ? kEdgePadding : windowWidth - rowWidth;
  for (const auto& [action, support] : shown) {
    layout.controls.push_back({action, QRectF(x, 0, hitWidth, kBarHeight), accessibleName(action),
                               support.available, support.reason});
    x += hitWidth;
  }

  layout.barHeight = kBarHeight;
  layout.barVisible = config.titleBarStyle == TitleBarStyle::Default;
  layout.contentInset = layout.barVisible ? kBarHeight : 0;
  if (!layout.barVisible) return layout;

  if (mac) {
    // Centre on the window, keeping the same clearance on both sides so the
    // title never runs under the controls.
    const qreal reserve = kEdgePadding + rowWidth + kTitlePadding;
    const qreal width = std::max<qreal>(0, windowWidth - 2 * reserve);
    layout.titleRect = QRectF(width > 0 ? reserve : windowWidth / 2, 0, width, kBarHeight);
    layout.titleAlignment = Qt::AlignHCenter;
  } else {
    const qreal width = std::max<qreal>(0, windowWidth - rowWidth - 2 * kTitlePadding);
    layout.titleRect = QRectF(kTitlePadding, 0, width, kBarHeight);
    layout.titleAlignment = Qt::AlignLeft;
  }
  return layout;
}

QSize clampWindowSize(QSize requested, QSize available) {
  if (!available.isValid() || available.isEmpty()) return requested;
  return requested.boundedTo(available);
}

}  // namespace rnqnx::window
