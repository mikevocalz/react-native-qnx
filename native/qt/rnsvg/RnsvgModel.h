#pragma once

// Retained element model for react-native-svg on the Qt Quick renderer.
//
// These types carry already-parsed element props. The Fabric bridge converts
// react-native-svg's generated props into them once per commit, so painting and
// hit testing never touch folly::dynamic or React types. Everything here is
// plain Qt/STL and is shared by the native tests.

#include "RnsvgBasicTypes.h"
#include "filter/FilterModel.h"

#include <QColor>
#include <QImage>
#include <QPainterPath>
#include <QSet>
#include <QString>
#include <QTransform>
#include <memory>
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
  Pattern,
  Symbol,
  Marker,
  Text,
  TSpan,
  TextPath,
  Use,
  Image,
  ForeignObject,
  Filter,
  FilterPrimitive,  // every Fe* element; `ElementProps::primitive` says which
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
  std::optional<double> wordSpacing;
  std::optional<double> kerning;
  QString decoration;          // "", none, underline, overline, line-through, or a combination
  QString variantLigatures;    // "", normal, none
  QString fontStretch;         // "", normal, condensed, ...
  QString fontFeatureSettings;
};

// viewBox plus preserveAspectRatio, as Pattern, Symbol and Marker carry them.
struct ViewBox {
  bool set{false};
  double minX{0}, minY{0}, width{0}, height{0};
  QString align{QStringLiteral("xMidYMid")};
  int meetOrSlice{0};  // 0 meet, 1 slice, 2 none
};

// Text and TSpan positioning and measurement props (react-native-svg
// SvgTextCommonProps). Lists are per-character: entry i applies to the i-th
// addressable character of the element, and the last entry does not repeat.
struct TextProps {
  std::vector<Length> x, y, dx, dy;
  std::vector<double> rotate;
  Length textLength;
  QString lengthAdjust;  // "", spacing, spacingAndGlyphs
  Length baselineShift;  // numeric, or see baselineShiftKeyword
  QString baselineShiftKeyword;  // baseline, sub, super
  QString alignmentBaseline;     // "", baseline, middle, hanging, central, ...
  Length verticalAlign;
  Length inlineSize;
};

// TextPath's own props.
struct TextPathProps {
  QString href;
  QString side;      // left, right
  QString method;    // align, stretch
  QString midLine;   // smooth, sharp
  QString spacing;   // auto, exact
  Length startOffset;
};

struct ElementProps {
  Kind kind{Kind::Group};
  QString name;  // the SVG id
  double opacity{1.0};
  std::optional<QTransform> matrix;
  QString clipPath;
  QString mask;
  QString filter;        // `filter="url(#id)"`: the id
  QString markerStart, markerMid, markerEnd;
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
  // clipRule: -1 unset (SVG default, nonzero), 0 evenodd, 1 nonzero. The
  // package's generated default is 0, which cannot tell "unset" from
  // "evenodd"; scripts/qt-rnsvg-codegen.mjs regenerates the prop with -1 as
  // its default so the two differ.
  int clipRule{-1};
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
  // The vertices of `path` as the path data wrote them, for markers: Qt's
  // path loses which cubic segments came from one arc, and where `z` closed.
  std::vector<PathVertex> vertices;
  Length x, y, width, height, rx, ry, cx, cy, r, x1, y1, x2, y2;
  Length fx, fy;

  // Gradients.
  std::vector<GradientStop> stops;
  bool gradientUserSpace{false};
  std::optional<QTransform> gradientTransform;
  int spreadMethod{0};  // 0 pad, 1 reflect, 2 repeat

  // Mask.
  bool maskUserSpace{false};         // maskUnits
  bool maskContentUserSpace{true};   // maskContentUnits
  bool maskAlpha{false};             // maskType: alpha instead of luminance

  // Pattern.
  bool patternUserSpace{false};         // patternUnits
  bool patternContentUserSpace{true};   // patternContentUnits is userSpaceOnUse (default)
  std::optional<QTransform> patternTransform;

  // Pattern, Symbol and Marker viewBox.
  ViewBox viewBox;

  // Marker.
  Length refX, refY, markerWidth, markerHeight;
  bool markerStrokeWidthUnits{true};  // markerUnits: strokeWidth (default) or userSpaceOnUse
  QString orient;                     // "auto", "auto-start-reverse", an angle, or ""

  // Filter element.
  bool filterUserSpace{false};      // filterUnits (default objectBoundingBox)
  bool primitiveUserSpace{true};    // primitiveUnits (default userSpaceOnUse)

  // Text, TSpan and TextPath.
  QString content;
  FontSpec font;
  TextProps text;
  TextPathProps textPath;

  // Use.
  QString href;

  // Image.
  QString imageUri;  // the source `image` was decoded from
  QImage image;
  QString imageAlign{QStringLiteral("xMidYMid")};
  int imageMeetOrSlice{0};
  // The loader's state for imageUri; see image/ImageSource.h.
  int imageState{0};

  // FilterPrimitive: the parameters of one Fe* element.
  std::shared_ptr<const filter::Primitive> primitive;
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
