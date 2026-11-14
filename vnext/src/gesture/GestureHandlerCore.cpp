#include "react-native-qnx/gesture/GestureHandlerCore.h"

#include <algorithm>
#include <cmath>
#include <deque>
#include <limits>
#include <set>
#include <utility>

namespace rnqnx::gesture {

namespace {

struct Sample {
  double t, x, y;
};

struct Track {
  double startX{0}, startY{0}, startMs{0};
  double x{0}, y{0};
  std::deque<Sample> samples;

  void add(double nowMs, double px, double py) {
    x = px;
    y = py;
    samples.push_back({nowMs, px, py});
    while (samples.size() > 2 && nowMs - samples.front().t > 100.0) {
      samples.pop_front();
    }
  }
  void velocity(double& vx, double& vy) const {
    vx = vy = 0;
    if (samples.size() < 2) return;
    const auto& a = samples.front();
    const auto& b = samples.back();
    const double dt = b.t - a.t;
    if (dt <= 0) return;
    vx = (b.x - a.x) / dt * 1000.0;
    vy = (b.y - a.y) / dt * 1000.0;
  }
};

double get(const Config& c, const char* key, double fallback) {
  const auto it = c.find(key);
  return it == c.end() ? fallback : it->second;
}
bool has(const Config& c, const char* key) { return c.find(key) != c.end(); }

constexpr int kRight = 1, kLeft = 2, kUp = 4, kDown = 8;

}  // namespace

class Handler {
 public:
  std::string name;
  int tag{0};
  int viewTag{0};
  int actionType{0};
  Config cfg;
  Relations rel;
  State state{State::Undetermined};
  std::set<int> pointers;
  bool pendingActivate{false};
  bool supported{true};
  // Recognizer scratch.
  int taps{0};
  double deadline{-1};
  double lastUpMs{0};
  bool activatedByManager{false};
};

struct Registry::Impl {
  Sink sink;
  HitTest hitTest;
  std::map<int, std::unique_ptr<Handler>> handlers;
  std::map<int, Track> tracks;
  std::vector<std::string> unsupported;

  bool enabled(const Handler& h) const { return get(h.cfg, "enabled", 1) != 0; }

  bool simultaneous(const Handler& a, const Handler& b) const {
    auto has_tag = [](const std::vector<int>& v, int tag) { return std::find(v.begin(), v.end(), tag) != v.end(); };
    return has_tag(a.rel.simultaneous, b.tag) || has_tag(b.rel.simultaneous, a.tag);
  }

  bool waitsFor(const Handler& a, const Handler& b) const {
    auto has_tag = [](const std::vector<int>& v, int tag) { return std::find(v.begin(), v.end(), tag) != v.end(); };
    return has_tag(a.rel.waitFor, b.tag) || has_tag(b.rel.blocks, a.tag);
  }

  HandlerData snapshot(const Handler& h) const {
    HandlerData d;
    d.numberOfPointers = static_cast<int>(h.pointers.size());
    if (h.pointers.empty()) return d;
    const auto it = tracks.find(*h.pointers.begin());
    if (it == tracks.end()) return d;
    const Track& t = it->second;
    d.absoluteX = t.x;
    d.absoluteY = t.y;
    if (hitTest && h.viewTag != 0) {
      const ViewHit hit = hitTest(h.viewTag, t.x, t.y);
      d.x = hit.localX;
      d.y = hit.localY;
      d.pointerInside = hit.contains;
    }
    d.translationX = t.x - t.startX;
    d.translationY = t.y - t.startY;
    t.velocity(d.velocityX, d.velocityY);
    return d;
  }

  void emit(const Handler& h, State oldState, EventKind kind, int direction = 0) {
    if (!sink) return;
    GestureEvent e;
    e.handlerTag = h.tag;
    e.viewTag = h.viewTag;
    e.actionType = h.actionType;
    e.handlerName = h.name;
    e.state = h.state;
    e.oldState = oldState;
    e.kind = kind;
    e.data = snapshot(h);
    e.data.direction = direction;
    sink(e);
  }

  void set(Handler& h, State next, int direction = 0) {
    if (h.state == next) return;
    const State old = h.state;
    h.state = next;
    emit(h, old, EventKind::StateChange, direction);
  }

  bool blockedByOthers(const Handler& h) const {
    for (const auto& [tag, other] : handlers) {
      if (other.get() == &h || !waitsFor(h, *other)) continue;
      if (other->state == State::Began || other->state == State::Active) return true;
    }
    return false;
  }

  void fail(Handler& h) {
    if (h.state == State::Active) {
      set(h, State::Cancelled);
    } else if (h.state == State::Began || h.state == State::Undetermined) {
      set(h, State::Failed);
    }
    h.pendingActivate = false;
    settleWaiters();
  }

  /// A handler wants to become active. Returns true when it did.
  bool tryActivate(Handler& h, int direction = 0) {
    if (h.state != State::Began) return false;
    if (blockedByOthers(h)) {
      h.pendingActivate = true;
      return false;
    }
    h.pendingActivate = false;
    set(h, State::Active, direction);
    // Exclusive by default: everyone who shares a pointer and is not declared
    // simultaneous loses.
    for (auto& [tag, other] : handlers) {
      if (other.get() == &h) continue;
      if (other->state != State::Began && other->state != State::Active) continue;
      bool shares = false;
      for (int p : h.pointers) shares = shares || other->pointers.count(p) != 0;
      if (!shares || simultaneous(h, *other)) continue;
      fail(*other);
    }
    return true;
  }

  void settleWaiters() {
    bool again = true;
    while (again) {
      again = false;
      for (auto& [tag, h] : handlers) {
        if (h->pendingActivate && h->state == State::Began && !blockedByOthers(*h)) {
          if (tryActivate(*h)) {
            // Recognizers that complete on activation (tap) end right away.
            finishOnActivate(*h);
            again = true;
            break;
          }
        }
      }
    }
  }

  void finishOnActivate(Handler& h) {
    if (h.name == "TapGestureHandler" || h.name == "FlingGestureHandler") {
      if (h.state == State::Active) set(h, State::End);
    }
  }

  void resetIfIdle() {
    if (!tracks.empty()) return;
    for (auto& [tag, h] : handlers) {
      if (h->state == State::End || h->state == State::Failed || h->state == State::Cancelled) {
        h->state = State::Undetermined;
      }
      if (h->state == State::Undetermined) {
        h->taps = 0;
        h->deadline = -1;
        h->pendingActivate = false;
      }
    }
  }

  // ---- recognizers ----------------------------------------------------------

  void onDown(Handler& h, double now) {
    if (!h.supported) {
      set(h, State::Failed);
      return;
    }
    if (h.state == State::Undetermined) set(h, State::Began);
    if (h.state != State::Began) return;
    if (h.name == "LongPressGestureHandler") {
      h.deadline = now + get(h.cfg, "minDurationMs", 500);
    } else if (h.name == "TapGestureHandler") {
      h.deadline = now + get(h.cfg, "maxDurationMs", 500);
    } else if (h.name == "FlingGestureHandler") {
      h.deadline = now + 800;
    }
  }

  void onMove(Handler& h, double now) {
    if (h.state != State::Began && h.state != State::Active) return;
    const Track& t = tracks[*h.pointers.begin()];
    const double dx = t.x - t.startX, dy = t.y - t.startY;
    const double dist = std::hypot(dx, dy);
    if (h.name == "PanGestureHandler") {
      if (h.state == State::Active) {
        emit(h, State::Active, EventKind::Update);
        return;
      }
      const double fxs = get(h.cfg, "failOffsetXStart", -1e9), fxe = get(h.cfg, "failOffsetXEnd", 1e9);
      const double fys = get(h.cfg, "failOffsetYStart", -1e9), fye = get(h.cfg, "failOffsetYEnd", 1e9);
      if (dx < fxs || dx > fxe || dy < fys || dy > fye) {
        fail(h);
        return;
      }
      const bool offsets = has(h.cfg, "activeOffsetXStart") || has(h.cfg, "activeOffsetXEnd") ||
                           has(h.cfg, "activeOffsetYStart") || has(h.cfg, "activeOffsetYEnd");
      bool activate;
      if (offsets) {
        activate = dx <= get(h.cfg, "activeOffsetXStart", -1e9) || dx >= get(h.cfg, "activeOffsetXEnd", 1e9) ||
                   dy <= get(h.cfg, "activeOffsetYStart", -1e9) || dy >= get(h.cfg, "activeOffsetYEnd", 1e9);
      } else {
        activate = dist >= get(h.cfg, "minDist", 10);
      }
      if (activate) tryActivate(h);
    } else if (h.name == "TapGestureHandler" || h.name == "LongPressGestureHandler") {
      if (h.state == State::Began && dist > get(h.cfg, "maxDist", 10)) fail(h);
    } else if (h.name == "LongPressGestureHandler" && h.state == State::Active) {
      emit(h, State::Active, EventKind::Update);
    }
    (void)now;
  }

  void onUp(Handler& h, double now) {
    if (h.name == "PanGestureHandler") {
      if (h.state == State::Active) {
        set(h, State::End);
      } else if (h.state == State::Began) {
        fail(h);
      }
    } else if (h.name == "TapGestureHandler") {
      if (h.state != State::Began) return;
      if (now - tracks[*h.pointers.begin()].startMs > get(h.cfg, "maxDurationMs", 500)) {
        fail(h);
        return;
      }
      h.taps += 1;
      if (h.taps >= static_cast<int>(get(h.cfg, "numberOfTaps", 1))) {
        // Contact duration is satisfied. A completed tap waiting for an
        // exclusive predecessor must not expire before that handler's delay.
        h.deadline = -1;
        if (tryActivate(h)) finishOnActivate(h);
      } else {
        h.lastUpMs = now;
        h.deadline = now + get(h.cfg, "maxDelayMs", 500);
      }
    } else if (h.name == "LongPressGestureHandler") {
      if (h.state == State::Active) {
        set(h, State::End);
      } else if (h.state == State::Began) {
        fail(h);
      }
    } else if (h.name == "FlingGestureHandler") {
      if (h.state != State::Began) return;
      const Track& t = tracks[*h.pointers.begin()];
      double vx = 0, vy = 0;
      t.velocity(vx, vy);
      const int dir = static_cast<int>(get(h.cfg, "direction", kRight | kLeft | kUp | kDown));
      const double need = get(h.cfg, "minVelocity", 900);
      int fired = 0;
      if ((dir & kRight) && vx >= need) fired = kRight;
      else if ((dir & kLeft) && vx <= -need) fired = kLeft;
      else if ((dir & kDown) && vy >= need) fired = kDown;
      else if ((dir & kUp) && vy <= -need) fired = kUp;
      if (fired != 0 && tryActivate(h, fired)) {
        if (h.state == State::Active) set(h, State::End, fired);
      } else if (fired == 0) {
        fail(h);
      }
    } else if (h.name == "ManualGestureHandler") {
      if (h.state == State::Began) fail(h);
    }
  }
};

Registry::Registry() : impl_(std::make_unique<Impl>()) {}
Registry::~Registry() = default;

void Registry::setSink(Sink sink) { impl_->sink = std::move(sink); }
void Registry::setHitTest(HitTest hitTest) { impl_->hitTest = std::move(hitTest); }

void Registry::create(const std::string& name, int tag, const Config& config) {
  auto h = std::make_unique<Handler>();
  h->name = name;
  h->tag = tag;
  h->cfg = config;
  h->supported = name == "PanGestureHandler" || name == "TapGestureHandler" ||
                 name == "LongPressGestureHandler" || name == "FlingGestureHandler" ||
                 name == "ManualGestureHandler";
  if (!h->supported) impl_->unsupported.push_back(name);
  impl_->handlers[tag] = std::move(h);
}

void Registry::attach(int tag, int viewTag, int actionType) {
  if (auto it = impl_->handlers.find(tag); it != impl_->handlers.end()) {
    it->second->viewTag = viewTag;
    it->second->actionType = actionType;
  }
}
void Registry::detach(int tag) {
  if (auto it = impl_->handlers.find(tag); it != impl_->handlers.end()) it->second->viewTag = 0;
}
void Registry::setConfig(int tag, const Config& config) {
  if (auto it = impl_->handlers.find(tag); it != impl_->handlers.end()) it->second->cfg = config;
}
void Registry::setRelations(int tag, const Relations& relations) {
  if (auto it = impl_->handlers.find(tag); it != impl_->handlers.end()) it->second->rel = relations;
}
void Registry::drop(int tag) {
  auto it = impl_->handlers.find(tag);
  if (it == impl_->handlers.end()) return;
  Handler& h = *it->second;
  if (h.state == State::Active || h.state == State::Began) impl_->fail(h);
  impl_->handlers.erase(it);
  impl_->settleWaiters();
}

void Registry::setState(int tag, State state) {
  auto it = impl_->handlers.find(tag);
  if (it == impl_->handlers.end()) return;
  Handler& h = *it->second;
  switch (state) {
    case State::Active:
      impl_->tryActivate(h);
      break;
    case State::End:
      if (h.state == State::Active) impl_->set(h, State::End);
      else if (h.state == State::Began) impl_->fail(h);
      break;
    case State::Failed:
    case State::Cancelled:
      impl_->fail(h);
      break;
    default:
      break;
  }
}

void Registry::pointerDown(int id, double x, double y, double nowMs) {
  Track& t = impl_->tracks[id];
  t = Track{};
  t.startX = x;
  t.startY = y;
  t.startMs = nowMs;
  t.add(nowMs, x, y);
  if (!impl_->hitTest) return;
  for (auto& [tag, h] : impl_->handlers) {
    if (h->viewTag == 0 || !impl_->enabled(*h)) continue;
    if (h->state != State::Undetermined && h->state != State::Began) continue;
    // A handler that already has a pointer (multi-tap, pinch) only takes more
    // pointers while it is still undecided.
    const ViewHit hit = impl_->hitTest(h->viewTag, x, y);
    if (!hit.contains) continue;
    if (!h->pointers.empty() && h->name != "TapGestureHandler") continue;
    h->pointers.insert(id);
    impl_->onDown(*h, nowMs);
  }
}

void Registry::pointerMove(int id, double x, double y, double nowMs) {
  auto it = impl_->tracks.find(id);
  if (it == impl_->tracks.end()) return;
  it->second.add(nowMs, x, y);
  // Copy the tags: handlers may fail each other while we iterate.
  std::vector<int> tags;
  for (auto& [tag, h] : impl_->handlers) {
    if (h->pointers.count(id) != 0) tags.push_back(tag);
  }
  for (int tag : tags) {
    auto h = impl_->handlers.find(tag);
    if (h != impl_->handlers.end()) impl_->onMove(*h->second, nowMs);
  }
}

void Registry::pointerUp(int id, double x, double y, double nowMs) {
  auto it = impl_->tracks.find(id);
  if (it == impl_->tracks.end()) return;
  it->second.add(nowMs, x, y);
  std::vector<int> tags;
  for (auto& [tag, h] : impl_->handlers) {
    if (h->pointers.count(id) != 0) tags.push_back(tag);
  }
  for (int tag : tags) {
    auto h = impl_->handlers.find(tag);
    if (h != impl_->handlers.end()) impl_->onUp(*h->second, nowMs);
  }
  for (int tag : tags) {
    auto h = impl_->handlers.find(tag);
    if (h == impl_->handlers.end()) continue;
    // Taps keep waiting for a second contact; everything else is done with it.
    if (h->second->name != "TapGestureHandler" || h->second->state != State::Began) h->second->pointers.erase(id);
    else h->second->pointers.erase(id);
  }
  impl_->tracks.erase(id);
  impl_->resetIfIdle();
}

void Registry::pointerCancel(int id, double nowMs) {
  auto it = impl_->tracks.find(id);
  if (it == impl_->tracks.end()) return;
  std::vector<int> tags;
  for (auto& [tag, h] : impl_->handlers) {
    if (h->pointers.count(id) != 0) tags.push_back(tag);
  }
  for (int tag : tags) {
    auto h = impl_->handlers.find(tag);
    if (h == impl_->handlers.end()) continue;
    h->second->pointers.erase(id);
    impl_->fail(*h->second);
  }
  impl_->tracks.erase(id);
  impl_->resetIfIdle();
  (void)nowMs;
}

void Registry::advance(double nowMs) {
  std::vector<int> tags;
  for (auto& [tag, h] : impl_->handlers) {
    if (h->deadline >= 0 && nowMs >= h->deadline) tags.push_back(tag);
  }
  for (int tag : tags) {
    auto it = impl_->handlers.find(tag);
    if (it == impl_->handlers.end()) continue;
    Handler& h = *it->second;
    h.deadline = -1;
    if (h.state != State::Began) continue;
    if (h.name == "LongPressGestureHandler") {
      if (impl_->tryActivate(h)) impl_->emit(h, State::Active, EventKind::Update);
    } else if (h.name == "TapGestureHandler" || h.name == "FlingGestureHandler") {
      impl_->fail(h);
    }
  }
  impl_->resetIfIdle();
}

std::optional<double> Registry::nextDeadlineMs() const {
  std::optional<double> next;
  for (const auto& [tag, h] : impl_->handlers) {
    if (h->deadline >= 0 && h->state == State::Began && (!next || h->deadline < *next)) next = h->deadline;
  }
  return next;
}

State Registry::stateOf(int tag) const {
  const auto it = impl_->handlers.find(tag);
  return it == impl_->handlers.end() ? State::Undetermined : it->second->state;
}
const std::vector<std::string>& Registry::unsupported() const { return impl_->unsupported; }
bool Registry::hasActivePointers() const { return !impl_->tracks.empty(); }

}  // namespace rnqnx::gesture
