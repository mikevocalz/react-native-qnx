#include "FixtureManifest.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace rnqnx::views::rnsvg::testing {

QByteArray readFileBytes(const QString& path) {
  QFile f(path);
  return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

std::vector<Fixture> loadFixtureManifest(const QString& dir, QString* error) {
  std::vector<Fixture> out;
  QJsonParseError pe;
  const QJsonDocument doc = QJsonDocument::fromJson(readFileBytes(dir + QStringLiteral("/manifest.json")), &pe);
  if (!doc.isObject()) {
    *error = QStringLiteral("manifest.json in %1: %2").arg(dir, pe.errorString());
    return out;
  }
  const QJsonObject defaults = doc.object().value(QStringLiteral("defaults")).toObject();
  for (const QJsonValue& v : doc.object().value(QStringLiteral("fixtures")).toArray()) {
    const QJsonObject o = v.toObject();
    Fixture f;
    f.name = o.value(QStringLiteral("name")).toString();
    f.svgPath = dir + QLatin1Char('/') + o.value(QStringLiteral("svg")).toString();
    if (o.value(QStringLiteral("reference")).toString() != QLatin1String("none")) {
      f.goldenPath = dir + QLatin1Char('/') + o.value(QStringLiteral("golden")).toString();
    }
    f.status = o.value(QStringLiteral("status")).toString();
    f.reason = o.value(QStringLiteral("reason")).toString();
    f.css = o.value(QStringLiteral("css")).toBool();
    const QJsonObject overrides = o.value(QStringLiteral("thresholds")).toObject();
    // A fixture's own value wins; the manifest default fills the rest.
    auto pick = [&](const char* key) {
      return overrides.contains(QLatin1String(key)) ? overrides.value(QLatin1String(key)) : defaults.value(QLatin1String(key));
    };
    f.thresholds.channelThreshold = pick("channelThreshold").toInt(24);
    f.thresholds.maxDiffPercent = pick("maxDiffPixelPercent").toDouble(1.5);
    f.thresholds.maxMeanAbsDiff = pick("meanAbsDiff").toDouble(2.0);
    f.thresholds.maxBlockPixels = pick("maxBlockPixels").toInt(400);
    f.thresholds.maxShiftTolerantPercent = pick("maxShiftTolerantPercent").toDouble(0.8);
    f.thresholds.maxShiftTolerantBlock = pick("maxShiftTolerantBlock").toInt(60);
    out.push_back(std::move(f));
  }
  return out;
}

}  // namespace rnqnx::views::rnsvg::testing
