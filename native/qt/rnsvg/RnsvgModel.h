#pragma once

// Retained element model for react-native-svg on the Qt Quick renderer.
//
// These types carry already-parsed element props. The Fabric bridge converts
// react-native-svg's generated props into them once per commit, so painting and
// hit testing never touch folly::dynamic or React types. Everything here is
// plain Qt/STL and is shared by the native tests.

#include <QColor>
#include <QImage>
#include <QPainterPath>
#include <QSet>
#include <QString>
#include <QTransform>
#include <optional>
#include <vector>

namespace rnqnx::views::rnsvg {

enum class Kind {
  Group,
  Defs,
  Path,
  Circle,
  Ellipse,
  Rect,
  Line,
  LinearGradient,
  RadialGradient,
  ClipPath,
  Mask,
  Text,
  TSpan,
  Use,
  Image,
};

// A react-native-svg NumberProp: a number, or a string with a unit.
struct Length {
  enum class Unit { Px, Percent, Em };
  double value{0};
  Unit unit{Unit::Px};
  bool set{false};
};

// react-native-svg extractBrush(): type 0 color, 1 url(#ref), 2 currentColor,
// 3 context-fill, 4 context-stroke; `none` is a null prop.
struct Brush {
  enum class Type { None, Color, Ref, CurrentColor, ContextFill, ContextStroke };
  Type type{Type::None};
  QColor color;
  QString ref;
};

struct GradientStop {
  double offset{0};
  QColor color;
};

struct FontSpec {
  QString family;
  std::optional<double> size;
  std::optional<int> weight;
  std::optional<bool> italic;
  QString anchor;  // "", start, middle, end
  std::optional<double> letterSpacing;
  QString decoration;
};

struct ElementProps {
  Kind kind{Kind::Group};
  QString name;  // the SVG id
  double opacity{1.0};
  std::optional<QTransform> matrix;
  QString clipPath;
  QString mask;
  bool responsible{false};
  bool displayNone{false};
  QString pointerEvents;
  std::optional<QColor> color;  // `currentColor`
  // Props named here were set on this element; every other paint prop is
  // inherited from the nearest ancestor that set it.
  QSet<QString> propList;
  Brush fill;
  Brush stroke;
  double fillOpacity{1.0};
  double strokeOpacity{1.0};
  bool fillEvenOdd{false};
  bool clipEvenOdd{false};
  Length strokeWidth;
  int strokeLinecap{0};   // 0 butt, 1 round, 2 square
  int strokeLinejoin{0};  // 0 miter, 1 round, 2 bevel
  std::vector<Length> strokeDasharray;
  double strokeDashoffset{0};
  double strokeMiterlimit{0};
  bool nonScalingStroke{false};

  // Geometry. Which fields apply depends on `kind`.
  QString pathData;   // the `d` string `path` was parsed from
  QPainterPath path;  // `d`, parsed
  Length x, y, width, height, rx, ry, cx, cy, r, x1, y1, x2, y2;
  Length fx, fy;

  // Gradients.
  std::vector<GradientStop> stops;
  bool gradientUserSpace{false};
  std::optional<QTransform> gradientTransform;

  // Mask.
  bool maskUserSpace{false};         // maskUnits
  bool maskContentUserSpace{true};   // maskContentUnits
  bool maskAlpha{false};             // maskType: alpha instead of luminance

  // Text and TSpan.
  QString content;
  FontSpec font;
  std::vector<Length> tx, ty, tdx, tdy;

  // Use.
  QString href;

  // Image.
  QString imageUri;  // the source `image` was decoded from
  QImage image;
  QString imageAlign{QStringLiteral("xMidYMid")};
  int imageMeetOrSlice{0};
};

// The <Svg> root's own props.
struct RootProps {
  Length bbWidth, bbHeight;
  double minX{0}, minY{0}, vbWidth{0}, vbHeight{0};
  QString align{QStringLiteral("xMidYMid")};
  int meetOrSlice{0};  // 0 meet, 1 slice, 2 none
  QColor color{Qt::black};
};

}  // namespace rnqnx::views::rnsvg
