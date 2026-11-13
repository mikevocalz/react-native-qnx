#include "WebGpuItem.h"

#include <QQuickWindow>
#include <QSGSimpleTextureNode>
#include <QSGTexture>

WebGpuItem::WebGpuItem(QQuickItem* parent) : QQuickItem(parent) { setFlag(ItemHasContents, true); }

void WebGpuItem::setFrame(QImage image) {
  frame_ = std::move(image);
  dirty_ = true;
  update();
}

QSGNode* WebGpuItem::updatePaintNode(QSGNode* old, UpdatePaintNodeData*) {
  auto* node = static_cast<QSGSimpleTextureNode*>(old);
  if (!node && frame_.isNull()) return nullptr;  // nothing to show before the first frame
  if (!node) {
    node = new QSGSimpleTextureNode();
    node->setOwnsTexture(true);
    node->setFiltering(QSGTexture::Nearest);
  }
  if (dirty_ && !frame_.isNull()) {
    // Uploads the frame into a texture owned by Qt's (OpenGL) scene graph.
    node->setTexture(window()->createTextureFromImage(frame_));
    dirty_ = false;
  }
  node->setRect(boundingRect());
  return node;
}
