#pragma once

// Pure window-chrome decisions: configuration parsing, per-QPA control
// availability, title bar layout and initial-size clamping. No Qt Quick here,
// so the rules are testable without a window. See docs/adr/0016.

#include <QJsonObject>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QStringList>

#include <optional>
#include <vector>

namespace rnqnx::window {

enum class ControlsLayout { MacOS, Windows };
enum class TitleBarStyle { Default, Hidden, None };
enum class WindowAction { Close, Minimize, Maximize };

struct WindowChromeConfig {
  ControlsLayout controls{ControlsLayout::MacOS};
  TitleBarStyle titleBarStyle{TitleBarStyle::Default};
  std::optional<QSize> initialSize;
};

// Largest edge PR #122's config plugin accepts (MAX_WINDOW_EDGE).
inline constexpr int kMaxWindowEdge = 16384;

// Parses the resolved `window` object that prepare-native writes into
// native-app.json. Throws std::invalid_argument on unknown keys or values, the
// same rules as resolveQnxWindow in @rn-qnx/expo-qnx-config-plugins.
WindowChromeConfig parseWindowChromeConfig(const QJsonObject& window);
ControlsLayout parseControlsLayout(const QString& value);
TitleBarStyle parseTitleBarStyle(const QString& value);
QString nameOf(ControlsLayout value);
QString nameOf(TitleBarStyle value);
QString nameOf(WindowAction value);

struct ActionSupport {
  bool available{false};
  QString reason;  // Why it is hidden; empty when available.
};

struct WindowCapabilities {
  QString platform;
  ActionSupport minimize;
  ActionSupport maximize;
  bool systemMove{false};
};

// The QPA name is QGuiApplication::platformName(). `platformOptions` are the
// colon-separated options after the name (for example `no-fullscreen` in
// `qnx:no-fullscreen`); see qnxPlatformOptions().
WindowCapabilities capabilitiesForPlatform(const QString& platform,
                                           const QStringList& platformOptions = {});

// True when the QPA will force the root window to the screen size whatever
// size the app asks for (the qnx plugin without `no-fullscreen`).
bool platformForcesFullScreen(const QString& platform, const QStringList& platformOptions);

// Extracts the QPA options from `-platform <spec>` / `--platform <spec>` /
// `-platform=<spec>` in argv, falling back to QT_QPA_PLATFORM. Must run before
// QGuiApplication removes `-platform` from argv.
QStringList platformOptionsFromArguments(int argc, const char* const* argv,
                                         const QString& environmentSpec);

struct ControlSlot {
  WindowAction action;
  QRectF hitRect;  // Device-independent pixels, window coordinates.
  QString accessibleName;
};

struct HiddenControl {
  WindowAction action;
  QString reason;
};

struct ChromeLayout {
  bool barVisible{false};      // Background and title drawn.
  qreal barHeight{0};          // Height of the bar or of the floating controls row.
  qreal contentInset{0};       // Where the Fabric root starts.
  QRectF titleRect;            // Empty when no title is drawn.
  Qt::Alignment titleAlignment{Qt::AlignHCenter};
  std::vector<ControlSlot> controls;
  std::vector<HiddenControl> hidden;
};

// Sizes in device-independent pixels. Qt multiplies them by the device pixel
// ratio. 44 is the smallest edge used for a finger target.
inline constexpr qreal kBarHeight = 44;
inline constexpr qreal kMacHitWidth = 44;
inline constexpr qreal kWindowsHitWidth = 46;
inline constexpr qreal kEdgePadding = 4;
inline constexpr qreal kTitlePadding = 12;

ChromeLayout layoutChrome(const WindowChromeConfig& config,
                          const WindowCapabilities& capabilities,
                          qreal windowWidth);

// Clamps a requested outer window size to the available screen area. A null or
// empty `available` (no screen) leaves the request unchanged.
QSize clampWindowSize(QSize requested, QSize available);

}  // namespace rnqnx::window
