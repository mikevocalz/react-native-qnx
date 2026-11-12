#pragma once

#include "MotionTypes.h"

#include <QQuickTransform>

namespace rnqnx::motion {

/// Private presentation transform. Does not write QQuickItem layout fields.
class MotionTransform final : public QQuickTransform {
 public:
  explicit MotionTransform(QObject* parent = nullptr);
  void setValue(const Transform& value);
  void applyTo(QMatrix4x4* matrix) const override;

 private:
  Transform value_;
};

} // namespace rnqnx::motion
