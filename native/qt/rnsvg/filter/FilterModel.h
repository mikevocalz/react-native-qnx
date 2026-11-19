#pragma once

// The retained description of one <filter> primitive, as the Fabric bridge
// builds it from an Fe* element's props. It carries SVG attribute values and
// no pixels: filter/FilterEngine.h turns a list of these into an image.
//
// Contract between the bridge (which fills these in) and the engine (which
// reads them). Adding a field is fine; renaming or removing one needs both
// sides updated.

#include "../RnsvgBasicTypes.h"

#include <QColor>
#include <QString>
#include <variant>
#include <vector>

namespace rnqnx::views::rnsvg::filter {

enum class PrimitiveType {
  Blend,
  ColorMatrix,
  Composite,
  Flood,
  GaussianBlur,
  Merge,
  Offset,
  Morphology,
  Turbulence,
  DisplacementMap,
  ComponentTransfer,
  DropShadow,
  Image,
  ConvolveMatrix,
  Tile,
  DiffuseLighting,
  SpecularLighting,
};

// color-interpolation-filters. SVG's default is linearRGB.
enum class ColorSpace { LinearRGB, SRGB };

enum class EdgeMode { Duplicate, Wrap, None };

// An input name as written: "" (the previous result, or SourceGraphic for the
// first primitive), SourceGraphic, SourceAlpha, BackgroundImage,
// BackgroundAlpha, FillPaint, StrokePaint, or the `result` of an earlier
// primitive.
using InputName = QString;

struct Blend {
  InputName in2;
  QString mode{QStringLiteral("normal")};  // normal, multiply, screen, darken, lighten, overlay, ...
};

struct ColorMatrix {
  QString type{QStringLiteral("matrix")};  // matrix, saturate, hueRotate, luminanceToAlpha
  std::vector<double> values;
};

struct Composite {
  InputName in2;
  QString op{QStringLiteral("over")};  // over, in, out, atop, xor, arithmetic
  double k1{0}, k2{0}, k3{0}, k4{0};
};

struct Flood {
  Brush color{Brush::Type::Color, QColor(Qt::black), {}};  // flood-color; currentColor allowed
  double opacity{1.0};
};

struct GaussianBlur {
  double stdDeviationX{0}, stdDeviationY{0};
  EdgeMode edgeMode{EdgeMode::None};
};

struct Merge {
  std::vector<InputName> nodes;
};

struct Offset {
  Length dx, dy;  // user units, or fractions of the bounding box in objectBoundingBox primitive units
};

struct Morphology {
  QString op{QStringLiteral("erode")};  // erode, dilate
  Length radiusX, radiusY;
};

struct Turbulence {
  double baseFrequencyX{0}, baseFrequencyY{0};
  int numOctaves{1};
  double seed{0};
  bool stitchTiles{false};
  QString type{QStringLiteral("turbulence")};  // turbulence, fractalNoise
};

struct DisplacementMap {
  InputName in2;
  double scale{0};
  QString xChannelSelector{QStringLiteral("A")};  // R, G, B, A
  QString yChannelSelector{QStringLiteral("A")};
};

// One of feFuncR/G/B/A.
struct TransferFunction {
  QString type{QStringLiteral("identity")};  // identity, table, discrete, linear, gamma
  std::vector<double> tableValues;
  double slope{1}, intercept{0}, amplitude{1}, exponent{1}, offset{0};
};

struct ComponentTransfer {
  TransferFunction r, g, b, a;
};

struct DropShadow {
  Length dx{2, Length::Unit::Px, true}, dy{2, Length::Unit::Px, true};
  double stdDeviationX{2}, stdDeviationY{2};
  Brush color{Brush::Type::Color, QColor(Qt::black), {}};
  double opacity{1.0};
};

struct Image {
  QString href;
  QString align{QStringLiteral("xMidYMid")};
  int meetOrSlice{0};  // 0 meet, 1 slice, 2 none
};

struct ConvolveMatrix {
  int orderX{3}, orderY{3};
  std::vector<double> kernelMatrix;
  double divisor{0};  // 0: the sum of the kernel, or 1 if that sum is 0
  double bias{0};
  int targetX{-1}, targetY{-1};  // -1: floor(order / 2)
  EdgeMode edgeMode{EdgeMode::Duplicate};
  bool preserveAlpha{false};
};

struct Tile {};

// feDistantLight, fePointLight or feSpotLight.
struct LightSource {
  enum class Kind { Distant, Point, Spot };
  Kind kind{Kind::Distant};
  double azimuth{0}, elevation{0};              // distant
  double x{0}, y{0}, z{0};                      // point, spot
  double pointsAtX{0}, pointsAtY{0}, pointsAtZ{0};  // spot
  double specularExponent{1};                   // spot
  double limitingConeAngle{0};                  // spot; 0 means no cone
  bool hasLimitingCone{false};
};

struct DiffuseLighting {
  double surfaceScale{1};
  double diffuseConstant{1};
  Brush lightingColor{Brush::Type::Color, QColor(Qt::white), {}};
  LightSource light;
};

struct SpecularLighting {
  double surfaceScale{1};
  double specularConstant{1};
  double specularExponent{1};
  Brush lightingColor{Brush::Type::Color, QColor(Qt::white), {}};
  LightSource light;
};

using Params = std::variant<Blend, ColorMatrix, Composite, Flood, GaussianBlur, Merge, Offset, Morphology,
                            Turbulence, DisplacementMap, ComponentTransfer, DropShadow, Image, ConvolveMatrix,
                            Tile, DiffuseLighting, SpecularLighting>;

// The primitive subregion attributes: unset means "the filter region".
struct Subregion {
  Length x, y, width, height;
};

// `params` decides which primitive this is; `type` is informational and the
// engine does not read it.
struct Primitive {
  PrimitiveType type{PrimitiveType::Offset};
  Subregion subregion;
  QString result;
  InputName in1;
  ColorSpace colorSpace{ColorSpace::LinearRGB};
  Params params;
};

}  // namespace rnqnx::views::rnsvg::filter
