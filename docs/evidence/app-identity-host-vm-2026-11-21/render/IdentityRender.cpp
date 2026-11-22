// Evidence tool, not shipped and not built by CI. Host Qt offscreen.
//   identity_render source <out.png>
//       draws a 1024x1024 sample icon with square corners (a stand-in for an app's own icon)
//   identity_render sheet <icons-dir> <out.png>
//       contact sheet of the generated icon-<size>.png files, each at 1:1
//   identity_render tiles <font-dir> <icon.png> <out.png> <out.json> <label>...
//       Marathon-derived launcher tile grid: cell 98.4 px (92.4 box + 6 padding), icon 86 px,
//       one-line label box 92.4 px in Source Sans 3 20 px with ElideRight. Numbers come from
//       docs/getting-started/app-identity.md; this is a mock of that derived box, not the launcher.
#include "LabelFit.h"
#include <QDir>
#include <QFile>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <cstdio>
using namespace rnqnx::identity;

static int source(const QString& out) {
  QImage img(1024, 1024, QImage::Format_ARGB32);
  QPainter p(&img);
  p.setRenderHint(QPainter::Antialiasing);
  QLinearGradient g(0, 0, 1024, 1024);
  g.setColorAt(0, QColor("#0b7a75"));
  g.setColorAt(1, QColor("#1b3a5c"));
  p.fillRect(img.rect(), g);
  p.setPen(Qt::NoPen);
  p.setBrush(QColor("#f4c542"));
  p.drawEllipse(QPointF(512, 512), 330, 330);  // mark stays inside the 66% circle (r = 337)
  p.setPen(QColor("#10283f"));
  QFont f("Source Sans 3");
  f.setPixelSize(360);
  f.setBold(true);
  p.setFont(f);
  p.drawText(QRectF(0, 0, 1024, 1024), Qt::AlignCenter, "FN");
  p.end();
  return img.save(out) ? 0 : 1;
}

static int sheet(const QString& dir, const QString& out) {
  const int sizes[] = {16, 24, 32, 48, 64, 96, 128, 192, 256, 512};
  int w = 20, h = 0;
  for (int s : sizes) { w += s + 20; h = qMax(h, s); }
  QImage img(w, h + 40, QImage::Format_ARGB32);
  img.fill(QColor("#e8e8e8"));
  QPainter p(&img);
  int x = 20;
  for (int s : sizes) {
    QImage icon(QDir(dir).filePath(QString("icon-%1.png").arg(s)));
    if (icon.isNull() || icon.width() != s || icon.height() != s) return 2;
    p.drawImage(x, 20, icon);
    x += s + 20;
  }
  p.end();
  return img.save(out) ? 0 : 1;
}

static int tiles(const QString& fontDir, const QString& iconPath, const QString& out, const QString& json,
    const QStringList& labels) {
  const QStringList files{fontDir + "/SourceSans3-Regular.ttf", fontDir + "/SourceSans3-Light.ttf"};
  LabelBox box{registerFontFiles(files), 20.0, 92.4, 2.0};
  QImage icon(iconPath);
  const qreal cellW = 98.4, iconPx = 86, labelW = 92.4;
  QFont font("Source Sans 3");
  font.setPixelSize(20);
  font.setHintingPreference(QFont::PreferNoHinting);
  const qreal lineH = QFontMetricsF(font).height();
  const qreal cellH = iconPx + 4 + lineH;
  const qreal pad = 16, gap = 8;
  const int n = labels.size();
  const qreal scale = 3;  // draw at 3x so the label is legible; sizes below are logical px
  QImage img(int((pad * 2 + n * cellW + (n - 1) * gap) * scale), int((pad * 2 + cellH + 44) * scale), QImage::Format_ARGB32);
  img.fill(QColor("#14181f"));
  QPainter p(&img);
  p.setRenderHint(QPainter::Antialiasing);
  p.setRenderHint(QPainter::SmoothPixmapTransform);
  p.scale(scale, scale);
  QJsonArray rows;
  for (int i = 0; i < n; ++i) {
    const qreal x = pad + i * (cellW + gap), y = pad;
    p.setPen(QPen(QColor("#3a4556"), 0.5));
    p.setBrush(Qt::NoBrush);
    p.drawRect(QRectF(x, y, cellW, cellH));  // the tile cell
    p.drawImage(QRectF(x + (cellW - iconPx) / 2, y, iconPx, iconPx), icon);
    const QRectF labelRect(x + (cellW - labelW) / 2, y + iconPx + 4, labelW, lineH);
    p.setPen(QPen(QColor("#6a3d3d"), 0.5, Qt::DotLine));
    p.drawRect(labelRect);  // the label box
    p.setPen(QColor("#f2f4f8"));
    p.setFont(font);
    const QFontMetricsF fm(font);
    p.drawText(labelRect, Qt::AlignHCenter | Qt::AlignVCenter | Qt::TextSingleLine,
        fm.elidedText(labels[i], Qt::ElideRight, labelW));
    const auto m = measureLabel(labels[i], box);
    QFont cap("Source Sans 3");
    cap.setPixelSize(9);
    p.setFont(cap);
    p.setPen(m.fits() ? QColor("#7ed492") : QColor("#f08a8a"));
    p.drawText(QRectF(x - 4, y + cellH + 6, cellW + 8, 30), Qt::AlignHCenter | Qt::AlignTop,
        QString("%1 px %2").arg(m.widthPx, 0, 'f', 1).arg(m.fits() ? "fits" : QString("over by %1").arg(m.overflowPx(), 0, 'f', 1)));
    rows.append(QJsonObject{{"label", labels[i]}, {"widthPx", m.widthPx}, {"fits", m.fits()},
        {"overflowPx", m.overflowPx()}, {"cellW", cellW}, {"cellH", cellH}, {"iconPx", iconPx},
        {"labelBoxW", labelRect.width()}, {"labelBoxH", labelRect.height()},
        {"elided", fm.elidedText(labels[i], Qt::ElideRight, labelW) != labels[i]}});
  }
  p.end();
  QFile f(json);
  if (!f.open(QIODevice::WriteOnly)) return 1;
  f.write(QJsonDocument(QJsonObject{{"qtVersion", qVersion()}, {"lineHeightPx", lineH}, {"tiles", rows}}).toJson());
  return img.save(out) ? 0 : 1;
}

int main(int argc, char** argv) {
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) qputenv("QT_QPA_PLATFORM", "offscreen");
  QGuiApplication app(argc, argv);
  const QStringList a = QCoreApplication::arguments();
  if (a.value(1) == "source" && a.size() == 3) return source(a[2]);
  if (a.value(1) == "sheet" && a.size() == 4) return sheet(a[2], a[3]);
  if (a.value(1) == "tiles" && a.size() >= 7) return tiles(a[2], a[3], a[4], a[5], a.mid(6));
  std::fprintf(stderr, "bad invocation\n");
  return 2;
}
