#include "MotionTransform.h"

#include <QMatrix4x4>

namespace rnqnx::motion {

MotionTransform::MotionTransform(QObject* parent) : QQuickTransform(parent) {}

void MotionTransform::setValue(const Transform& value) {
  value_ = value;
  update();
}

void MotionTransform::applyTo(QMatrix4x4* matrix) const {
  matrix->translate(static_cast<float>(value_.translateX + value_.originX),
                    static_cast<float>(value_.translateY + value_.originY));
  matrix->rotate(static_cast<float>(value_.rotationDegrees), 0.0F, 0.0F, 1.0F);
  matrix->scale(static_cast<float>(value_.scaleX), static_cast<float>(value_.scaleY));
  matrix->translate(static_cast<float>(-value_.originX), static_cast<float>(-value_.originY));
}

} // namespace rnqnx::motion
