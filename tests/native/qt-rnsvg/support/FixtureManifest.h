#pragma once

// tests/fixtures/rnsvg/manifest.json as the two fixture runners read it: the
// host runner (RnsvgFixturesTest) and the real-path runner in qt-rnsvg-fabric.

#include "ImageDiff.h"

#include <QString>
#include <vector>

namespace rnqnx::views::rnsvg::testing {

struct Fixture {
  QString name;
  QString svgPath;
  QString goldenPath;  // empty when the fixture has no reference image
  QString status;      // pass, pending:<feature>, reference-none
  QString reason;
  bool css{false};     // the document has a <style> block and goes through SvgCss
  DiffThresholds thresholds;

  bool pending() const { return status.startsWith(QLatin1String("pending:")); }
  QString pendingFeature() const { return pending() ? status.mid(8) : QString(); }
};

// Reads `<dir>/manifest.json`. `error` is empty on success.
std::vector<Fixture> loadFixtureManifest(const QString& dir, QString* error);

QByteArray readFileBytes(const QString& path);

}  // namespace rnqnx::views::rnsvg::testing
