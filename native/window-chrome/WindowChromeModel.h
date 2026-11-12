#pragma once

// Pure window-chrome decisions: configuration parsing, per-QPA control
// availability, title bar layout and initial-size clamping. No Qt Quick here,
// so the rules are testable without a window. See docs/adr/0016.

#include <QJsonObject>
#include <QPoint>
#include <QRect>
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
// What to do with a control the platform cannot run. `Hide` leaves it out
// (the default). `Disable` draws it greyed out and inert, so a macOS-style bar
// keeps all three lights on a platform such as QNX where only close works.
enum class UnavailableControls { Hide, Disable };

struct WindowChromeConfig {
  ControlsLayout controls{ControlsLayout::MacOS};
  TitleBarStyle titleBarStyle{TitleBarStyle::Default};
  std::optional<QSize> initialSize;
  UnavailableControls unavailableControls{UnavailableControls::Hide};
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
UnavailableControls parseUnavailableControls(const QString& value);

struct ActionSupport {
  bool available{false};
  QString reason;  // Why it is hidden; empty when available.
};

struct WindowCapabilities {
  QString platform;
  ActionSupport minimize;
  ActionSupport maximize;
  bool systemMove{false};
  // The window manager keeps the native window at the display size, so the
  // app draws its window frame inside it and moves, resizes and maximizes
  // that frame itself (QNX with fullscreen-winmgr, which sets every window to
  // the display size each time it posts a frame). See WindowChrome.h.
  bool containedWindow{false};
};

// Whether the host resizes the React surface when the window changes size.
// QtReactHost starts the surface once with a fixed size, so today it does not,
// and maximize stays hidden: it would grow the window around a surface that
// keeps its first size.
enum class SurfaceResize { Fixed, FollowsWindow };

// The QPA name is QGuiApplication::platformName(). `platformOptions` are the
// colon-separated options after the name (for example `no-fullscreen` in
// `qnx:no-fullscreen`); see platformOptionsFor().
WindowCapabilities capabilitiesForPlatform(const QString& platform,
                                           const QStringList& platformOptions = {},
                                           SurfaceResize surface = SurfaceResize::Fixed);

// True when the QPA will force the root window to the screen size whatever
// size the app asks for (the qnx plugin without `no-fullscreen`).
bool platformForcesFullScreen(const QString& platform, const QStringList& platformOptions);

// The QPA specs Qt will try, in order: the `-platform <spec>` /
// `--platform <spec>` / `-platform=<spec>` argument if present, otherwise the
// `;`-separated fallback list in QT_QPA_PLATFORM. Must run before
// QGuiApplication removes `-platform` from argv.
QStringList platformSpecsFromArguments(int argc, const char* const* argv,
                                       const QString& environmentSpec);

// The options of the first spec whose name is `platform` (the QPA Qt actually
// loaded, QGuiApplication::platformName()). Empty when none matches.
QStringList platformOptionsFor(const QString& platform, const QStringList& specs);

// The QT_QPA_PLATFORM value that lets a QNX app open at its own size: every
// `qnx` entry gains `no-fullscreen` (kept once). An empty value becomes
// `qnx:no-fullscreen` when `defaultIsQnx` (a QNX build, where qnx is the
// default plugin). Other plugins are returned unchanged.
QString windowedPlatformSpec(const QString& environmentSpec, bool defaultIsQnx);

struct ControlSlot {
  WindowAction action;
  QRectF hitRect;  // Device-independent pixels, window coordinates.
  QString accessibleName;
  bool enabled{true};
  QString reason;  // Why it is disabled; empty when enabled.
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
  std::vector<HiddenControl> disabled;  // Drawn but inert (UnavailableControls::Disable).
};

// Sizes in device-independent pixels. Qt multiplies them by the device pixel
// ratio. 44 is the smallest edge used for a finger target.
inline constexpr qreal kBarHeight = 44;
inline constexpr qreal kMacHitWidth = 44;
inline constexpr qreal kWindowsHitWidth = 46;
inline constexpr qreal kEdgePadding = 4;
inline constexpr qreal kTitlePadding = 12;
// Client-side move and resize (WindowCapabilities::clientMoveResize).
inline constexpr qreal kResizeBorder = 6;   // Edge band that resizes.
inline constexpr qreal kResizeCorner = 16;  // Corner reach along each edge.
inline constexpr int kMinWindowWidth = 400;
inline constexpr int kMinWindowHeight = 300;
inline constexpr int kKeepVisible = 80;     // Bar pixels kept on screen when moving.

// Which frame edges a pointer at `point` (frame coordinates) would resize:
// within kResizeBorder of an edge, inside or outside the frame, and within
// kResizeCorner of a corner for both edges. Empty elsewhere and on a control.
Qt::Edges resizeEdgesAt(QPointF point, QSizeF frame, const std::vector<QRectF>& controls);
// `start` dragged by `delta` on `edges`, no smaller than `minimum` and inside
// `screen` (an invalid screen does not constrain). The opposite edges stay put.
QRect resizedGeometry(const QRect& start, Qt::Edges edges, QPoint delta, QSize minimum,
                      const QRect& screen);
// `start` moved by `delta`, keeping the bar inside `screen` vertically and at
// least kKeepVisible pixels of it horizontally.
QRect movedGeometry(const QRect& start, QPoint delta, const QRect& screen);

ChromeLayout layoutChrome(const WindowChromeConfig& config,
                          const WindowCapabilities& capabilities,
                          qreal windowWidth);

// Clamps a requested outer window size to the available screen area. A null or
// empty `available` (no screen) leaves the request unchanged.
QSize clampWindowSize(QSize requested, QSize available);

}  // namespace rnqnx::window
