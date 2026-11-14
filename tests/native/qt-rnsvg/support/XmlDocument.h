#pragma once

// A small element tree for the fixture loader, parsed the way react-native-svg's
// own parser (src/xml.tsx) sees a document: attribute names are camel-cased
// ("stroke-width" -> strokeWidth, "xlink:href" -> xlinkHref), the `style`
// attribute is split into declarations, comments and processing instructions
// are dropped, and text that is only whitespace is dropped.

#include <QHash>
#include <QString>
#include <QVector>
#include <memory>
#include <variant>
#include <vector>

namespace rnqnx::views::rnsvg::testing {

struct XmlNode;
using XmlChild = std::variant<std::unique_ptr<XmlNode>, QString>;

struct XmlNode {
  QString tag;
  QHash<QString, QString> attrs;                // camel-cased names, raw values
  QVector<std::pair<QString, QString>> style;   // declarations in source order, camel-cased
  XmlNode* parent{nullptr};
  std::vector<XmlChild> children;

  QString attr(const QString& name) const { return attrs.value(name); }
  bool has(const QString& name) const { return attrs.contains(name); }
  // The value of a `style` declaration, or an empty string.
  QString styleValue(const QString& name) const;
};

struct XmlParseResult {
  std::unique_ptr<XmlNode> root;
  QString error;  // empty on success
};

// Nesting deeper than this is rejected, so hostile input cannot overflow the
// stack of the recursive loader.
inline constexpr int kMaxXmlDepth = 256;

XmlParseResult parseXml(const QByteArray& source);

// xml.tsx camelCase(): "a-b" and "a:b" become "aB".
QString camelCase(const QString& name);

// xml.tsx getStyle(): "fill: red; stroke:blue" -> declarations.
QVector<std::pair<QString, QString>> parseStyleAttribute(const QString& text);

}  // namespace rnqnx::views::rnsvg::testing
