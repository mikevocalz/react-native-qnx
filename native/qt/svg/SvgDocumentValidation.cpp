#include "SvgDocumentValidation.h"
#include "SvgDocument.h"
#include "SvgPathValidation.h"
#include "SvgTransformValidation.h"

#include <QColor>
#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QStringDecoder>
#include <QXmlStreamReader>
#include <cmath>

namespace rnqnx::views::detail {
namespace {
using Names = QSet<QString>;

const QHash<QString, Names>& attributesByElement() {
  static const QHash<QString, Names> values{
      {"svg", {"width", "height", "viewBox", "version"}}, {"g", {}}, {"defs", {}},
      {"path", {"d"}}, {"rect", {"x", "y", "width", "height", "rx", "ry"}},
      {"circle", {"cx", "cy", "r"}}, {"ellipse", {"cx", "cy", "rx", "ry"}},
      {"line", {"x1", "y1", "x2", "y2"}}, {"polyline", {"points"}}, {"polygon", {"points"}},
      {"linearGradient", {"x1", "y1", "x2", "y2", "gradientUnits", "gradientTransform", "spreadMethod"}},
      {"radialGradient", {"cx", "cy", "r", "fx", "fy", "gradientUnits", "gradientTransform", "spreadMethod"}},
      {"stop", {"offset", "stop-color", "stop-opacity"}}, {"title", {}}, {"desc", {}}};
  return values;
}

const Names& presentationAttributes() {
  static const Names values{"id", "transform", "fill", "fill-opacity", "fill-rule", "stroke",
      "stroke-width", "stroke-opacity", "stroke-linecap", "stroke-linejoin", "stroke-miterlimit",
      "stroke-dasharray", "stroke-dashoffset", "opacity", "color", "display", "visibility"};
  return values;
}

bool finiteNumber(QString value, bool percentage = false, bool pixels = false) {
  if ((percentage && value.endsWith('%')) || (pixels && value.endsWith("px"))) {
    value.chop(value.endsWith('%') ? 1 : 2);
  }
  bool ok = false;
  const double number = value.toDouble(&ok);
  return ok && std::isfinite(number) && std::abs(number) <= kSvgMaxCoordinate;
}

std::optional<QVector<double>> numberList(const QString& value) {
  const auto words = value.split(QRegularExpression("[,\\s]+"), Qt::SkipEmptyParts);
  QVector<double> values;
  if (words.isEmpty() || words.size() > 4096) return std::nullopt;
  for (const auto& word : words) {
    if (!finiteNumber(word)) return std::nullopt;
    values.append(word.toDouble());
  }
  return values;
}

bool validPaint(const QString& value, QSet<QString>& references) {
  static const QRegularExpression local("^url\\(#([A-Za-z_][A-Za-z0-9_.:-]{0,127})\\)$");
  const auto reference = local.match(value);
  if (reference.hasMatch()) {
    references.insert(reference.captured(1));
    return true;
  }
  return value == "none" || value == "currentColor" || QColor::isValidColorName(value);
}

bool validAttribute(const QString& element, const QString& name, const QString& value,
                    QSet<QString>& references) {
  static const Names numbers{"x", "y", "width", "height", "rx", "ry", "cx", "cy", "r", "fx", "fy",
      "x1", "y1", "x2", "y2", "stroke-width", "stroke-miterlimit", "stroke-dashoffset", "offset"};
  if (name == "id") return QRegularExpression("^[A-Za-z_][A-Za-z0-9_.:-]{0,127}$").match(value).hasMatch();
  if (name == "fill" || name == "stroke") return validPaint(value, references);
  if (name == "color" || name == "stop-color") return value == "currentColor" || QColor::isValidColorName(value);
  if (name.endsWith("opacity")) return finiteNumber(value) && value.toDouble() >= 0 && value.toDouble() <= 1;
  if (name == "d") return isSupportedSvgPath(value);
  if (name == "points") {
    const auto points = numberList(value);
    return points && points->size() >= (element == "polygon" ? 6 : 4) && points->size() % 2 == 0;
  }
  if (name == "transform" || name == "gradientTransform") return parseSvgTransform(value).has_value();
  if (name == "stroke-dasharray") {
    if (value == "none") return true;
    const auto numbers = numberList(value);
    if (!numbers) return false;
    bool positive = false;
    for (double number : *numbers) {
      if (number < 0) return false;
      positive = positive || number > 0;
    }
    return positive;
  }
  if (name == "viewBox") {
    const auto box = numberList(value);
    return box && box->size() == 4 && (*box)[2] >= kSvgMinDimension && (*box)[3] >= kSvgMinDimension &&
        (*box)[2] <= kSvgMaxDimension && (*box)[3] <= kSvgMaxDimension;
  }
  if (numbers.contains(name)) {
    if (!finiteNumber(value, element.endsWith("Gradient") || name == "offset", true)) return false;
    QString numeric = value;
    const bool percentage = numeric.endsWith('%');
    if (percentage || numeric.endsWith("px")) numeric.chop(percentage ? 1 : 2);
    const double number = numeric.toDouble();
    if (name == "width" || name == "height" || name == "r" || name == "rx" || name == "ry" || name == "stroke-width") return number >= 0;
    if (name == "stroke-miterlimit") return number >= 1;
    if (name == "offset") return number >= 0 && number <= (percentage ? 100 : 1);
    return true;
  }
  if (name == "fill-rule") return value == "nonzero" || value == "evenodd";
  if (name == "stroke-linecap") return value == "butt" || value == "round" || value == "square";
  if (name == "stroke-linejoin") return value == "miter" || value == "round" || value == "bevel";
  if (name == "gradientUnits") return value == "userSpaceOnUse" || value == "objectBoundingBox";
  if (name == "spreadMethod") return value == "pad" || value == "reflect" || value == "repeat";
  if (name == "display") return value == "none" || value == "inline";
  if (name == "visibility") return value == "visible" || value == "hidden" || value == "collapse";
  return name == "version" && (value == "1.0" || value == "1.1" || value == "1.2");
}

bool validParent(const QString& child, const QString& parent) {
  if (parent.isEmpty()) return child == "svg";
  if (child == "svg") return false;
  if (child == "title" || child == "desc") return parent != "title" && parent != "desc";
  if (child == "stop") return parent.endsWith("Gradient");
  if (child.endsWith("Gradient")) return parent == "defs" || parent == "svg";
  return parent == "svg" || parent == "g";
}
}  // namespace

std::optional<QString> validateSvgDocument(const QByteArray& utf8Xml) {
  if (utf8Xml.size() > kSvgMaxDocumentBytes) return QStringLiteral("SVG source exceeds 65536 bytes");
  QStringDecoder decoder(QStringDecoder::Utf8);
  const QString xml = decoder(utf8Xml);
  if (decoder.hasError() || xml.contains(QChar::Null)) return QStringLiteral("SVG source must be UTF-8 XML without NUL bytes");
  QXmlStreamReader reader(xml);
  QStringList parents;
  QVector<QTransform> transforms;
  QSet<QString> ids, gradients, references;
  int elements = 0;
  bool hasRoot = false;
  while (!reader.atEnd()) {
    const auto token = reader.readNext();
    if (token == QXmlStreamReader::DTD || token == QXmlStreamReader::EntityReference ||
        token == QXmlStreamReader::ProcessingInstruction) {
      return QStringLiteral("SVG DTDs, entities, and processing instructions are unsupported");
    }
    if (token == QXmlStreamReader::StartElement) {
      const QString element = reader.name().toString();
      if (++elements > kSvgMaxElements || parents.size() >= kSvgMaxDepth) return QStringLiteral("SVG exceeds the 512 element or 32 nesting-level limit");
      if (!attributesByElement().contains(element) || !validParent(element, parents.isEmpty() ? QString() : parents.last())) {
        return QStringLiteral("Unsupported SVG element or nesting: %1").arg(element);
      }
      const auto space = reader.namespaceUri();
      if ((!space.isEmpty() && space != QLatin1String("http://www.w3.org/2000/svg")) || !reader.prefix().isEmpty()) {
        return QStringLiteral("SVG custom namespaces and prefixed elements are unsupported");
      }
      for (const auto& declaration : reader.namespaceDeclarations()) {
        if (declaration.namespaceUri() != QLatin1String("http://www.w3.org/2000/svg") &&
            declaration.namespaceUri() != QLatin1String("http://www.w3.org/1999/xlink")) {
          return QStringLiteral("SVG custom namespace declarations are unsupported");
        }
      }
      const auto attributes = reader.attributes();
      if (attributes.size() > kSvgMaxAttributes) return QStringLiteral("SVG element exceeds the 32 attribute limit");
      for (const auto& attribute : attributes) {
        const QString name = attribute.name().toString();
        const QString value = attribute.value().toString().trimmed();
        if (!attribute.namespaceUri().isEmpty() || !attribute.prefix().isEmpty() ||
            (!attributesByElement().value(element).contains(name) && !presentationAttributes().contains(name))) {
          return QStringLiteral("Unsupported SVG attribute: %1").arg(attribute.qualifiedName());
        }
        if (value.size() > kSvgMaxAttributeCharacters || !validAttribute(element, name, value, references)) {
          return QStringLiteral("Invalid or unsupported SVG %1 attribute").arg(name);
        }
        if (name == "id") {
          if (ids.contains(value)) return QStringLiteral("SVG IDs must be unique");
          ids.insert(value);
          if (element.endsWith("Gradient")) gradients.insert(value);
        }
      }
      if (element == "svg") {
        if (hasRoot) return QStringLiteral("SVG must have exactly one root");
        hasRoot = true;
        if (!attributes.hasAttribute("viewBox") && !(attributes.hasAttribute("width") && attributes.hasAttribute("height"))) {
          return QStringLiteral("SVG requires a positive viewBox or width and height");
        }
        for (const auto* name : {"width", "height"}) {
          if (attributes.hasAttribute(name)) {
            QString value = attributes.value(name).toString().trimmed();
            if (value.endsWith("px")) value.chop(2);
            if (!finiteNumber(value) || value.toDouble() < kSvgMinDimension || value.toDouble() > kSvgMaxDimension) {
              return QStringLiteral("SVG root dimensions must be between 0.000001 and 16384");
            }
          }
        }
      }
      QTransform transform = transforms.isEmpty() ? QTransform() : transforms.last();
      if (attributes.hasAttribute("transform")) {
        const auto local = parseSvgTransform(attributes.value("transform").toString().trimmed());
        if (!local) return QStringLiteral("Invalid SVG transform");
        transform = *local * transform;
        if (!isBoundedSvgTransform(transform)) return QStringLiteral("SVG nested transform exceeds the numeric limits");
      }
      parents.append(element);
      transforms.append(transform);
    } else if (token == QXmlStreamReader::EndElement) {
      if (!parents.isEmpty()) parents.removeLast();
      if (!transforms.isEmpty()) transforms.removeLast();
    } else if (token == QXmlStreamReader::Characters && !reader.isWhitespace() &&
               (parents.isEmpty() || (parents.last() != "title" && parents.last() != "desc"))) {
      return QStringLiteral("SVG text is supported only in title and desc; convert visible text to paths");
    }
  }
  if (reader.hasError() || !hasRoot) return QStringLiteral("Malformed SVG XML: %1").arg(reader.errorString());
  if (!references.subtract(gradients).isEmpty()) return QStringLiteral("SVG paint references must resolve to a declared local gradient");
  return std::nullopt;
}

}  // namespace rnqnx::views::detail
