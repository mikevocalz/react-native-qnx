#pragma once

#include <QMetaType>

namespace rnqnx::motion {

enum class Track { Opacity, Transform };
enum class Easing { Linear, InQuad, OutQuad, InOutCubic };
enum class Completion { Finished, Cancelled, Interrupted, Detached, TargetDestroyed };
enum class State { Idle, Running, Paused, Detached };

struct Timing {
  int durationMs{250};
  Easing easing{Easing::InOutCubic};
};

/// An extra local transform, independent of Yoga geometry and the item's
/// existing scale/rotation. Translation/origin are logical pixels.
struct Transform {
  double translateX{0};
  double translateY{0};
  double scaleX{1};
  double scaleY{1};
  double rotationDegrees{0};
  double originX{0};
  double originY{0};
};

} // namespace rnqnx::motion

Q_DECLARE_METATYPE(rnqnx::motion::Track)
Q_DECLARE_METATYPE(rnqnx::motion::Completion)
