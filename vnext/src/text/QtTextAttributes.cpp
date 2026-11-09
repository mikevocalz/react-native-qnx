#include "react-native-qnx/text/QtTextAttributes.h"

#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QHash>
#include <QResource>
#include <QStandardPaths>
#include <QStringList>

#include <algorithm>
#include <cmath>

// Q_INIT_RESOURCE must be outside a namespace so its generated symbol has the
// same linkage as Qt's resource object in the renderer target.
static void initializeTextFontResources() {
  Q_INIT_RESOURCE(rnqnx_text_fonts);
}

namespace rnqnx::text {
namespace {

const QString& bundledSansFamily() {
  // C++ initializes this once across Fabric measurement and Qt mounting
  // threads. QFontDatabase's application-font API is thread-safe. The fonts
  // stay registered for the application lifetime; the cached string owns no
  // Qt object and does not depend on QGuiApplication's teardown order.
  static const QString family = [] {
    initializeTextFontResources();
    const int regular = QFontDatabase::addApplicationFont(
        QStringLiteral(":/rnqnx/fonts/Roboto-Regular.ttf"));
    const auto families = QFontDatabase::applicationFontFamilies(regular);
    if (families.isEmpty()) return QString{};
    QFontDatabase::addApplicationFont(QStringLiteral(":/rnqnx/fonts/Roboto-Italic.ttf"));
    return families.first();
  }();
  return family;
}

qreal fontSizeMultiplier(const facebook::react::TextAttributes& attributes) {
  if (!attributes.allowFontScaling.value_or(true)) return 1.0;
  const qreal multiplier = std::isfinite(attributes.fontSizeMultiplier) &&
          attributes.fontSizeMultiplier > 0
      ? attributes.fontSizeMultiplier
      : 1.0;
  return std::isfinite(attributes.maxFontSizeMultiplier) &&
          attributes.maxFontSizeMultiplier >= 1.0
      ? (std::min)(multiplier, static_cast<qreal>(attributes.maxFontSizeMultiplier))
      : multiplier;
}

QString resolveFontFamily(const QString& requested) {
  // Fabric may measure away from the mounting thread. A per-thread cache
  // avoids shared mutable bookkeeping; QFontDatabase itself is thread-safe.
  thread_local QHash<QString, QString> resolved;
  const auto cached = resolved.constFind(requested);
  if (cached != resolved.constEnd()) return cached.value();

  QStringList paths = {
      QCoreApplication::applicationDirPath() + QStringLiteral("/assets/fonts"),
      QCoreApplication::applicationDirPath() + QStringLiteral("/fonts")};
  paths.append(QStandardPaths::standardLocations(QStandardPaths::FontsLocation));
  for (const auto& directory : paths) {
    for (const auto& extension : {".ttf", ".otf", ".ttc"}) {
      for (const auto& family : {requested, requested.toLower()}) {
        const QString path = QDir(directory).filePath(family + QLatin1String(extension));
        if (!QFile::exists(path)) continue;
        const int id = QFontDatabase::addApplicationFont(path);
        const auto families = QFontDatabase::applicationFontFamilies(id);
        if (!families.isEmpty()) {
          resolved.insert(requested, families.first());
          return families.first();
        }
      }
    }
  }
  resolved.insert(requested, requested);
  return requested;
}

qreal lineHeight(const facebook::react::TextAttributes& attributes) {
  return std::isfinite(attributes.lineHeight) && attributes.lineHeight > 0
      ? attributes.lineHeight * fontSizeMultiplier(attributes)
      : 0.0;
}

}  // namespace

QFont fontFromTextAttributes(const facebook::react::TextAttributes& attributes) {
  QFont font = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
  const QString generalFamily = font.family();
  const QString& sansFamily = bundledSansFamily();
  const QString requested = QString::fromStdString(attributes.fontFamily);
  if (requested.isEmpty() || requested == QLatin1String("system-ui") ||
      requested == QLatin1String("system") || requested == QLatin1String("sans-serif")) {
    // The deployed resource supplies a sans face even on a minimal QNX image.
    // Use the family Qt actually registered, retaining platform fallback if a
    // platform font engine cannot load the resource.
    QStringList families = {QStringLiteral("sans-serif"), generalFamily};
    if (!sansFamily.isEmpty()) families.prepend(sansFamily);
    font.setFamilies(families);
    font.setStyleHint(QFont::SansSerif);
  } else if (requested == QLatin1String("monospace")) {
    font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setStyleHint(QFont::Monospace);
  } else {
    QStringList families = {resolveFontFamily(requested)};
    if (!sansFamily.isEmpty()) families.append(sansFamily);
    families.append(generalFamily);
    families.removeDuplicates();
    font.setFamilies(families);
  }

  const qreal size = std::isfinite(attributes.fontSize) && attributes.fontSize > 0
      ? attributes.fontSize
      : facebook::react::TextAttributes::defaultTextAttributes().fontSize;
  // QFont/Qt Quick expose integer pixel sizes. Round once AFTER font scaling
  // instead of truncating first or mixing physical DPI-dependent point sizes.
  const qreal scaledSize = std::clamp(size * fontSizeMultiplier(attributes), 1.0, 65535.0);
  font.setPixelSize(static_cast<int>(std::lround(scaledSize)));
  font.setStyleName(QString());
  font.setWeight(attributes.fontWeight.has_value()
      ? static_cast<QFont::Weight>(static_cast<int>(*attributes.fontWeight))
      : QFont::Normal);
  font.setItalic(attributes.fontStyle == facebook::react::FontStyle::Italic);
  font.setLetterSpacing(QFont::AbsoluteSpacing,
      std::isfinite(attributes.letterSpacing) ? attributes.letterSpacing : 0.0);
  return font;
}

qreal paragraphLineHeight(const facebook::react::AttributedString& attributedString) {
  qreal height = lineHeight(attributedString.getBaseTextAttributes());
  for (const auto& fragment : attributedString.getFragments()) {
    if (!fragment.isAttachment()) height = (std::max)(height, lineHeight(fragment.textAttributes));
  }
  return height;
}

QString fragmentMarkup(const facebook::react::AttributedString::Fragment& fragment) {
  const auto& attributes = fragment.textAttributes;
  const QFont font = fontFromTextAttributes(attributes);
  QStringList families;
  for (QString family : font.families()) {
    family.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    family.replace(QLatin1Char('\''), QStringLiteral("\\'"));
    families.append(QStringLiteral("'%1'").arg(family));
  }
  QStringList styles = {
      QStringLiteral("font-family:%1").arg(families.join(QLatin1Char(','))),
      QStringLiteral("font-size:%1px").arg(font.pixelSize()),
      QStringLiteral("font-weight:%1").arg(static_cast<int>(font.weight())),
      QStringLiteral("font-style:%1").arg(font.italic() ? "italic" : "normal"),
      QStringLiteral("letter-spacing:%1px").arg(font.letterSpacing()),
      QStringLiteral("white-space:pre-wrap")};
  if (facebook::react::isColorMeaningful(attributes.foregroundColor)) {
    const auto& color = attributes.foregroundColor;
    styles.append(QStringLiteral("color:%1").arg(QColor(
        facebook::react::redFromColor(color),
        facebook::react::greenFromColor(color),
        facebook::react::blueFromColor(color)).name(QColor::HexRgb)));
  }
  QString content = QString::fromStdString(fragment.string).toHtmlEscaped();
  content.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
  content.replace(QLatin1Char('\n'), QStringLiteral("<br />"));
  return QStringLiteral("<span style=\"%1\">%2</span>")
      .arg(styles.join(QLatin1Char(';')).toHtmlEscaped(), content);
}

}  // namespace rnqnx::text
