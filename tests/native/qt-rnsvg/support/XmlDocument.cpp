#include "XmlDocument.h"

#include <QXmlStreamReader>

namespace rnqnx::views::rnsvg::testing {

QString XmlNode::styleValue(const QString& name) const {
  for (const auto& [key, value] : style) {
    if (key == name) return value;
  }
  return {};
}

QString camelCase(const QString& name) {
  QString out;
  out.reserve(name.size());
  for (qsizetype i = 0; i < name.size(); ++i) {
    const QChar c = name.at(i);
    if ((c == QLatin1Char('-') || c == QLatin1Char(':')) && i + 1 < name.size() && name.at(i + 1) >= QLatin1Char('a') &&
        name.at(i + 1) <= QLatin1Char('z')) {
      out.append(name.at(++i).toUpper());
    } else {
      out.append(c);
    }
  }
  return out;
}

QVector<std::pair<QString, QString>> parseStyleAttribute(const QString& text) {
  QVector<std::pair<QString, QString>> out;
  const QStringList declarations = text.split(QLatin1Char(';'));
  for (const QString& declaration : declarations) {
    if (declaration.trimmed().isEmpty()) continue;
    const QStringList parts = declaration.split(QLatin1Char(':'));
    // xml.tsx keeps parts[0] and parts[1]; a value holding a colon is cut there.
    if (parts.size() < 2) continue;
    out.push_back({camelCase(parts[0].trimmed()), parts[1].trimmed()});
  }
  return out;
}

namespace {

// Whitespace-only text is dropped, as xml.tsx drops it; other text keeps its
// surrounding whitespace.
void flushText(XmlNode* current, QString& text) {
  if (current && !text.trimmed().isEmpty()) current->children.emplace_back(text);
  text.clear();
}

}  // namespace

XmlParseResult parseXml(const QByteArray& source) {
  XmlParseResult result;
  QXmlStreamReader xml(source);
  xml.setNamespaceProcessing(false);
  XmlNode* current = nullptr;
  QString text;
  int depth = 0;
  while (!xml.atEnd()) {
    switch (xml.readNext()) {
      case QXmlStreamReader::StartElement: {
        if (++depth > kMaxXmlDepth) {
          result.error = QStringLiteral("nesting deeper than %1").arg(kMaxXmlDepth);
          result.root.reset();
          return result;
        }
        flushText(current, text);
        auto node = std::make_unique<XmlNode>();
        node->tag = xml.name().toString();
        for (const auto& a : xml.attributes()) {
          const QString name = camelCase(a.qualifiedName().toString());
          const QString value = a.value().toString();
          if (name == QLatin1String("style")) node->style = parseStyleAttribute(value);
          node->attrs.insert(name, value);
        }
        XmlNode* raw = node.get();
        raw->parent = current;
        if (current) {
          current->children.emplace_back(std::move(node));
        } else if (!result.root) {
          result.root = std::move(node);
        } else {
          result.error = QStringLiteral("more than one root element");
          result.root.reset();
          return result;
        }
        current = raw;
        break;
      }
      case QXmlStreamReader::EndElement:
        flushText(current, text);
        --depth;
        current = current ? current->parent : nullptr;
        break;
      case QXmlStreamReader::Characters:
        // Qt delivers one run of text as several events around entities and
        // CDATA sections; xml.tsx sees it as one string.
        text += xml.text();
        break;
      default:
        break;
    }
  }
  if (xml.hasError()) {
    result.error = xml.errorString();
    result.root.reset();
  } else if (!result.root) {
    result.error = QStringLiteral("no root element");
  }
  return result;
}

}  // namespace rnqnx::views::rnsvg::testing
