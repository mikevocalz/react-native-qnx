// Host tool for `rnqnx project prepare-native` and `doctor`:
//   rnqnx_identity_tool icon  --source <png> --out-dir <dir> --size <px>...
//   rnqnx_identity_tool label --text-hex <utf8 hex> --font <ttf>...
//                             --pixel-size <px> --box-width <px> [--margin <px>]
// Prints one JSON object on stdout. Exit 0: measured or written; 1: rejected
// input (message on stderr); 2: invalid invocation.
#include "IconSet.h"
#include "LabelFit.h"

#include <QCommandLineParser>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <cstdio>
#include <stdexcept>

using namespace rnqnx::identity;

namespace {

void print(const QJsonObject& object) {
  std::printf("%s\n", QJsonDocument(object).toJson(QJsonDocument::Compact).constData());
}

qreal positive(const QCommandLineParser& parser, const QString& name) {
  bool ok = false;
  const qreal value = parser.value(name).toDouble(&ok);
  if (!ok || !(value > 0)) throw std::invalid_argument(("--" + name + " must be positive").toStdString());
  return value;
}

int runIcon(const QCommandLineParser& parser) {
  QList<int> sizes;
  for (const auto& text : parser.values(QStringLiteral("size"))) {
    bool ok = false;
    const int size = text.toInt(&ok);
    if (!ok) throw std::invalid_argument("--size must be an integer");
    sizes.append(size);
  }
  if (sizes.isEmpty() || !parser.isSet(QStringLiteral("source")) || !parser.isSet(QStringLiteral("out-dir"))) {
    throw std::invalid_argument("icon needs --source, --out-dir and at least one --size");
  }
  QJsonArray entries;
  for (const auto& icon : writeIconSet(parser.value(QStringLiteral("source")),
           parser.value(QStringLiteral("out-dir")), sizes)) {
    entries.append(QJsonObject{{QStringLiteral("size"), icon.size}, {QStringLiteral("path"), icon.path}});
  }
  print({{QStringLiteral("sizes"), entries}, {QStringLiteral("scaler"), QStringLiteral("QImage::scaled SmoothTransformation")},
      {QStringLiteral("qtVersion"), QString::fromLatin1(qVersion())}});
  return 0;
}

int runLabel(const QCommandLineParser& parser) {
  QStringList fonts;
  for (const auto& font : parser.values(QStringLiteral("font"))) fonts.append(QFileInfo(font).absoluteFilePath());
  if (fonts.isEmpty() || !parser.isSet(QStringLiteral("text-hex"))) {
    throw std::invalid_argument("label needs --text-hex and at least one --font");
  }
  const QByteArray utf8 = QByteArray::fromHex(parser.value(QStringLiteral("text-hex")).toLatin1());
  const QString text = QString::fromUtf8(utf8);
  if (text.toUtf8() != utf8) throw std::invalid_argument("--text-hex is not well-formed UTF-8");
  LabelBox box{registerFontFiles(fonts), positive(parser, QStringLiteral("pixel-size")),
      positive(parser, QStringLiteral("box-width"))};
  if (parser.isSet(QStringLiteral("margin"))) {
    bool ok = false;
    box.marginPx = parser.value(QStringLiteral("margin")).toDouble(&ok);
    if (!ok) throw std::invalid_argument("--margin must be a number");
  }
  const auto measured = measureLabel(text, box);
  QJsonArray faces;
  for (const auto& face : box.faces) faces.append(face.name());
  QJsonArray fallback;
  for (const char32_t codePoint : measured.fallbackCodePoints) {
    fallback.append(QStringLiteral("U+") +
        QString::number(quint32(codePoint), 16).toUpper().rightJustified(4, QLatin1Char('0')));
  }
  QJsonObject result{{QStringLiteral("widthPx"), measured.widthPx},
      {QStringLiteral("boxWidthPx"), measured.boxWidthPx},
      {QStringLiteral("overflowPx"), measured.overflowPx()},
      {QStringLiteral("marginPx"), measured.marginPx},
      {QStringLiteral("mightBeRichText"), measured.mightBeRichText},
      {QStringLiteral("fits"), measured.fits()},
      {QStringLiteral("lineCount"), measured.lineCount},
      {QStringLiteral("widestFace"), measured.widestFace},
      {QStringLiteral("faces"), faces},
      {QStringLiteral("pixelSize"), box.pixelSize},
      {QStringLiteral("rightToLeft"), measured.rightToLeft},
      {QStringLiteral("fallbackCodePoints"), fallback},
      {QStringLiteral("qtVersion"), QString::fromLatin1(qVersion())}};
  if (!measured.suggestion.isEmpty()) result.insert(QStringLiteral("suggestion"), measured.suggestion);
  print(result);
  return 0;
}

} // namespace

int main(int argc, char** argv) {
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) qputenv("QT_QPA_PLATFORM", "offscreen");
  QGuiApplication app(argc, argv);
  QCommandLineParser parser;
  parser.addPositionalArgument(QStringLiteral("command"), QStringLiteral("icon or label"));
  parser.addOptions({{QStringLiteral("source"), QStringLiteral("Square PNG source."), QStringLiteral("png")},
      {QStringLiteral("out-dir"), QStringLiteral("Output directory."), QStringLiteral("dir")},
      {QStringLiteral("size"), QStringLiteral("Icon edge in pixels; repeatable."), QStringLiteral("px")},
      {QStringLiteral("text-hex"), QStringLiteral("Label text as UTF-8 hex."), QStringLiteral("hex")},
      {QStringLiteral("font"), QStringLiteral("Launcher font file; repeatable."), QStringLiteral("ttf")},
      {QStringLiteral("pixel-size"), QStringLiteral("Launcher font pixel size."), QStringLiteral("px")},
      {QStringLiteral("box-width"), QStringLiteral("Label box width."), QStringLiteral("px")},
      {QStringLiteral("margin"), QStringLiteral("Width kept free inside the box."), QStringLiteral("px")}});
  if (!parser.parse(QCoreApplication::arguments()) || parser.positionalArguments().size() != 1) {
    std::fprintf(stderr, "rnqnx_identity_tool: %s\n",
        parser.errorText().isEmpty() ? "expected one command: icon or label" : parser.errorText().toUtf8().constData());
    return 2;
  }
  const QString command = parser.positionalArguments().first();
  try {
    if (command == QStringLiteral("icon")) return runIcon(parser);
    if (command == QStringLiteral("label")) return runLabel(parser);
    std::fprintf(stderr, "rnqnx_identity_tool: unknown command\n");
    return 2;
  } catch (const std::invalid_argument& error) {
    std::fprintf(stderr, "rnqnx_identity_tool: %s\n", error.what());
    return 2;
  } catch (const std::exception& error) {
    std::fprintf(stderr, "rnqnx_identity_tool: %s\n", error.what());
    return 1;
  }
}
