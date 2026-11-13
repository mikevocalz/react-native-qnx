#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace rnqnx::gesture {

/// Gesture Handler's state numbering (src/State.ts).
enum class State : int { Undetermined = 0, Failed = 1, Began = 2, Cancelled = 3, Active = 4, End = 5 };

/// What a handler reports, in the shape the JS payloads use.
struct HandlerData {
  double x{0}, y{0};                 // relative to the attached view
  double absoluteX{0}, absoluteY{0}; // window coordinates
  double translationX{0}, translationY{0};
  double velocityX{0}, velocityY{0}; // px per second
  int numberOfPointers{0};
  bool pointerInside{true};
  int duration{0};                   // ms, long press / tap
  int direction{0};                  // fling direction mask that fired
};

enum class EventKind { StateChange, Update };

struct GestureEvent {
  int handlerTag{0};
  int viewTag{0};
  int actionType{0};
  std::string handlerName;
  State state{State::Undetermined};
  State oldState{State::Undetermined};
  EventKind kind{EventKind::StateChange};
  HandlerData data;
};

/// Config values as Gesture Handler 3 sends them (flattened numbers and bools).
using Config = std::map<std::string, double>;

struct Relations {
  std::vector<int> simultaneous;
  std::vector<int> waitFor;
  std::vector<int> blocks;
};

/// Geometry of an attached view, supplied by the host at hit-test time.
struct ViewHit {
  bool contains{false};
  double localX{0}, localY{0};
};
using HitTest = std::function<ViewHit(int viewTag, double windowX, double windowY)>;

class Handler;

/// Recognizer registry and arbitration, independent of Qt.
///
/// All input is pointer based. The host feeds window-coordinate pointer
/// events; this decides which handlers begin, activate, fail or end, and
/// reports each transition and each update through the sink. Time comes from
/// the caller (`nowMs`), so tests are deterministic and a host can drive
/// long-press deadlines with its own timer via `nextDeadlineMs()`.
///
/// Implemented handlers: Pan, Tap, LongPress, Fling, Manual. Pinch, Rotation,
/// Native and Hover are accepted by `create` and immediately fail on the first
/// pointer; this is reported rather than silent (`unsupported()`).
class Registry {
 public:
  using Sink = std::function<void(const GestureEvent&)>;

  Registry();
  ~Registry();
  Registry(const Registry&) = delete;
  Registry& operator=(const Registry&) = delete;

  void setSink(Sink sink);
  void setHitTest(HitTest hitTest);

  void create(const std::string& name, int tag, const Config& config);
  void attach(int tag, int viewTag, int actionType);
  void detach(int tag);
  void setConfig(int tag, const Config& config);
  void setRelations(int tag, const Relations& relations);
  void drop(int tag);

  /// Manual handlers and `GestureStateManager`: activate, fail or end a handler.
  void setState(int tag, State state);

  void pointerDown(int pointerId, double x, double y, double nowMs);
  void pointerMove(int pointerId, double x, double y, double nowMs);
  void pointerUp(int pointerId, double x, double y, double nowMs);
  void pointerCancel(int pointerId, double nowMs);

  /// Fires long-press deadlines that have passed.
  void advance(double nowMs);
  std::optional<double> nextDeadlineMs() const;

  State stateOf(int tag) const;
  /// Names of handlers created that this registry cannot recognize.
  const std::vector<std::string>& unsupported() const;
  bool hasActivePointers() const;

 private:
  friend class Handler;
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace rnqnx::gesture
