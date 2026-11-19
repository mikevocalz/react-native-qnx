#pragma once

// The comparison with the resvg goldens.
// Each function is one test; RnsvgFilterTest.cpp runs it as a slot. The QtTest
// macros work in any void function, so these are plain functions.

#include "../UnitSupport.h"

#include <QDir>
#include <limits>

namespace filtertest {

inline void goldens() {
  const QString dir = QStringLiteral(RNQNX_RNSVG_TEST_DATA_DIR "/golden/filter/");
  QStringList failures;
  int checked = 0;
  for (const GoldenCase& g : goldenCases()) {
    QImage golden(dir + g.name + QStringLiteral(".png"));
    if (golden.isNull()) {
      failures << g.name + QStringLiteral(": golden missing");
      continue;
    }
    Fixture f(g.source);
    g.build(f);
    QElapsedTimer timer;
    timer.start();
    const QImage got = runOnWhite(f);
    const double ms = timer.nsecsElapsed() / 1.0e6;
    const Diff d = compare(got, golden.convertToFormat(QImage::Format_ARGB32_Premultiplied), g.threshold);
    ++checked;
    qInfo().noquote() << QStringLiteral("GOLDEN %1 max=%2 mean=%3 over=%4%  %5 ms")
                             .arg(g.name, -26).arg(d.maxDiff, 3).arg(d.mean, 6, 'f', 3).arg(d.overFraction * 100, 6, 'f', 3)
                             .arg(ms, 5, 'f', 2);
    // RNSVG_FILTER_DUMP=<dir> writes every rendering for inspection.
    const QString dump = qEnvironmentVariable("RNSVG_FILTER_DUMP");
    if (!dump.isEmpty()) got.save(dump + QLatin1Char('/') + g.name + QStringLiteral(".png"));
    if (d.overFraction > g.maxOver || d.mean > g.maxMean) {
      failures << QStringLiteral("%1: over=%2% (max %3%) mean=%4 (max %5)")
                      .arg(g.name).arg(d.overFraction * 100).arg(g.maxOver * 100).arg(d.mean).arg(g.maxMean);
    }
  }
  QVERIFY2(failures.isEmpty(), qPrintable(failures.join(QLatin1Char('\n'))));
  QVERIFY(checked >= 60);
}

}  // namespace filtertest
