// A Qt Quick item that shows the newest WebGPU frame as a scene-graph texture.
#pragma once

#include <QImage>
#include <QQuickItem>

class WebGpuItem : public QQuickItem {
  Q_OBJECT
 public:
  explicit WebGpuItem(QQuickItem* parent = nullptr);

 public Q_SLOTS:
  void setFrame(QImage image);

 protected:
  QSGNode* updatePaintNode(QSGNode* old, UpdatePaintNodeData*) override;

 private:
  QImage frame_;
  bool dirty_ = false;
};
