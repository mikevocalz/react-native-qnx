#include "FakeQnxScreen.h"

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

struct _screen_context {
  std::uint64_t id = 0;
  bool alive = false;
};

struct _screen_buffer {
  std::uint64_t id = 0;
  screen_window_t owner = nullptr;
  bool alive = false;
};

struct _screen_window {
  std::uint64_t id = 0;
  screen_context_t context = nullptr;
  int type = SCREEN_APPLICATION_WINDOW;
  int usage = 0;
  int format = 0;
  int bufferWidth = 0;
  int bufferHeight = 0;
  int width = 0;
  int height = 0;
  bool alive = false;
  std::vector<screen_buffer_t> renderBuffers;
};

namespace {

using qnx_screen_fake::FailurePoint;
using qnx_screen_fake::Function;

struct PendingFailure {
  FailurePoint point;
  int errorNumber;
};

struct Model {
  qnx_screen_fake::State observations;
  std::vector<std::unique_ptr<_screen_context>> contexts;
  std::vector<std::unique_ptr<_screen_window>> windows;
  std::vector<std::unique_ptr<_screen_buffer>> buffers;
  std::optional<PendingFailure> pendingFailure;
  std::optional<int> reportedRenderBufferCount;
  bool returnNullRenderBuffer = false;
  std::uint64_t nextId = 1;
};

Model& model() {
  static Model instance;
  return instance;
}

void record(Function function, int detail = 0) {
  model().observations.calls.push_back({function, detail});
}

bool failIfRequested(FailurePoint point) {
  auto& pending = model().pendingFailure;
  if (!pending || pending->point != point) {
    return false;
  }

  errno = pending->errorNumber;
  pending.reset();
  return true;
}

int failInvalidHandle() {
  ++model().observations.invalidHandleCalls;
  errno = EINVAL;
  return -1;
}

bool isAlive(screen_context_t context) {
  return context != nullptr && context->alive;
}

bool isAlive(screen_window_t window) {
  return window != nullptr && window->alive;
}

bool isAlive(screen_buffer_t buffer) {
  return buffer != nullptr && buffer->alive;
}

void releaseBuffers(screen_window_t window) {
  for (screen_buffer_t buffer : window->renderBuffers) {
    if (buffer != nullptr && buffer->alive) {
      buffer->alive = false;
      --model().observations.liveBuffers;
    }
  }
  window->renderBuffers.clear();
}

void releaseWindow(screen_window_t window) {
  releaseBuffers(window);
  window->alive = false;
  --model().observations.liveWindows;
}

int createWindow(
    screen_window_t* output,
    screen_context_t context,
    int type,
    bool usedTypedApi,
    FailurePoint failurePoint,
    Function function) {
  record(function, type);
  if (output == nullptr) {
    errno = EINVAL;
    return -1;
  }
  *output = nullptr;

  if (!isAlive(context)) {
    return failInvalidHandle();
  }
  if (failIfRequested(failurePoint)) {
    return -1;
  }

  auto window = std::make_unique<_screen_window>();
  window->id = model().nextId++;
  window->context = context;
  window->type = type;
  window->alive = true;
  *output = window.get();
  model().windows.push_back(std::move(window));
  ++model().observations.liveWindows;
  model().observations.windowCreations.push_back(
      {*output, context, type, usedTypedApi});
  return 0;
}

std::optional<FailurePoint> setWindowPropertyFailurePoint(int property) {
  switch (property) {
    case SCREEN_PROPERTY_USAGE:
      return FailurePoint::SetWindowUsage;
    case SCREEN_PROPERTY_FORMAT:
      return FailurePoint::SetWindowFormat;
    case SCREEN_PROPERTY_BUFFER_SIZE:
      return FailurePoint::SetWindowBufferSize;
    case SCREEN_PROPERTY_SIZE:
      return FailurePoint::SetWindowSize;
    default:
      return std::nullopt;
  }
}

std::vector<int> copyPropertyValue(int property, const int* value) {
  if (value == nullptr) {
    return {};
  }
  if (property == SCREEN_PROPERTY_BUFFER_SIZE || property == SCREEN_PROPERTY_SIZE) {
    return {value[0], value[1]};
  }
  return {value[0]};
}

qnx_screen_fake::FillCall copyFillCall(
    screen_context_t context,
    screen_buffer_t buffer,
    const int* attributes) {
  qnx_screen_fake::FillCall call{
      context,
      buffer,
      {},
      false,
      attributes == nullptr,
  };
  if (attributes == nullptr) {
    return call;
  }

  constexpr std::size_t kMaximumPairs = 32;
  std::size_t offset = 0;
  for (std::size_t pair = 0; pair < kMaximumPairs; ++pair) {
    const int token = attributes[offset++];
    call.attributes.push_back(token);
    if (token == SCREEN_BLIT_END) {
      call.attributesTerminated = true;
      break;
    }
    call.attributes.push_back(attributes[offset++]);
  }
  return call;
}

} // namespace

namespace qnx_screen_fake {

void reset() {
  model() = Model{};
}

void failNext(FailurePoint point, int errorNumber) {
  model().pendingFailure = PendingFailure{point, errorNumber};
}

void reportRenderBufferCount(int count) {
  model().reportedRenderBufferCount = count;
}

void returnNullRenderBufferOnce() {
  model().returnNullRenderBuffer = true;
}

const State& state() {
  return model().observations;
}

} // namespace qnx_screen_fake

extern "C" int screen_create_context(screen_context_t* output, int flags) {
  record(Function::CreateContext, flags);
  model().observations.contextFlags.push_back(flags);
  if (output == nullptr) {
    errno = EINVAL;
    return -1;
  }
  *output = nullptr;

  if (failIfRequested(FailurePoint::CreateContext)) {
    return -1;
  }

  auto context = std::make_unique<_screen_context>();
  context->id = model().nextId++;
  context->alive = true;
  *output = context.get();
  model().contexts.push_back(std::move(context));
  ++model().observations.liveContexts;
  return 0;
}

extern "C" int screen_destroy_context(screen_context_t context) {
  record(Function::DestroyContext);
  if (!isAlive(context)) {
    return failInvalidHandle();
  }

  // QNX Screen destroys render targets that remain associated with a context.
  // Explicit window destruction is still observed separately by this fake.
  for (const auto& window : model().windows) {
    if (window->alive && window->context == context) {
      releaseWindow(window.get());
    }
  }
  context->alive = false;
  --model().observations.liveContexts;
  model().observations.destroyedContexts.push_back(context);
  return 0;
}

extern "C" int screen_create_window_type(
    screen_window_t* output,
    screen_context_t context,
    int type) {
  return createWindow(
      output,
      context,
      type,
      true,
      FailurePoint::CreateWindowType,
      Function::CreateWindowType);
}

extern "C" int screen_destroy_window(screen_window_t window) {
  record(Function::DestroyWindow);
  if (!isAlive(window)) {
    return failInvalidHandle();
  }

  releaseWindow(window);
  model().observations.destroyedWindows.push_back(window);
  return 0;
}

extern "C" int screen_set_window_property_iv(
    screen_window_t window,
    int property,
    const int* value) {
  record(Function::SetWindowPropertyIv, property);
  model().observations.windowPropertyWrites.push_back(
      {window, property, copyPropertyValue(property, value)});
  if (!isAlive(window)) {
    return failInvalidHandle();
  }
  if (value == nullptr) {
    errno = EINVAL;
    return -1;
  }
  if (const auto failurePoint = setWindowPropertyFailurePoint(property);
      failurePoint && failIfRequested(*failurePoint)) {
    return -1;
  }

  switch (property) {
    case SCREEN_PROPERTY_USAGE:
      window->usage = value[0];
      return 0;
    case SCREEN_PROPERTY_FORMAT:
      window->format = value[0];
      return 0;
    case SCREEN_PROPERTY_BUFFER_SIZE:
      if (value[0] <= 0 || value[1] <= 0) {
        errno = EINVAL;
        return -1;
      }
      window->bufferWidth = value[0];
      window->bufferHeight = value[1];
      return 0;
    case SCREEN_PROPERTY_SIZE:
      if (value[0] <= 0 || value[1] <= 0) {
        errno = EINVAL;
        return -1;
      }
      window->width = value[0];
      window->height = value[1];
      return 0;
    default:
      errno = EINVAL;
      return -1;
  }
}

extern "C" int screen_get_window_property_iv(
    screen_window_t window,
    int property,
    int* value) {
  record(Function::GetWindowPropertyIv, property);
  model().observations.windowPropertyIntReads.push_back({window, property});
  if (!isAlive(window)) {
    return failInvalidHandle();
  }
  if (value == nullptr) {
    errno = EINVAL;
    return -1;
  }
  if (property != SCREEN_PROPERTY_RENDER_BUFFER_COUNT) {
    errno = EINVAL;
    return -1;
  }
  if (failIfRequested(FailurePoint::GetRenderBufferCount)) {
    return -1;
  }

  if (model().reportedRenderBufferCount) {
    *value = *model().reportedRenderBufferCount;
    model().reportedRenderBufferCount.reset();
  } else {
    *value = static_cast<int>(window->renderBuffers.size());
  }
  return 0;
}

extern "C" int screen_get_window_property_pv(
    screen_window_t window,
    int property,
    void** value) {
  record(Function::GetWindowPropertyPv, property);
  model().observations.windowPropertyPointerReads.push_back({window, property});
  if (!isAlive(window)) {
    return failInvalidHandle();
  }
  if (value == nullptr) {
    errno = EINVAL;
    return -1;
  }
  if (property != SCREEN_PROPERTY_RENDER_BUFFERS) {
    errno = EINVAL;
    return -1;
  }
  if (failIfRequested(FailurePoint::GetRenderBuffers)) {
    return -1;
  }

  for (std::size_t index = 0; index < window->renderBuffers.size(); ++index) {
    value[index] = window->renderBuffers[index];
  }
  if (model().returnNullRenderBuffer && !window->renderBuffers.empty()) {
    value[0] = nullptr;
    model().returnNullRenderBuffer = false;
  }
  return 0;
}

extern "C" int screen_create_window_buffers(screen_window_t window, int count) {
  record(Function::CreateWindowBuffers, count);
  model().observations.windowBufferCreations.push_back({window, count});
  if (!isAlive(window)) {
    return failInvalidHandle();
  }
  if (failIfRequested(FailurePoint::CreateWindowBuffers)) {
    return -1;
  }
  if (count < 0 || !window->renderBuffers.empty()) {
    errno = EINVAL;
    return -1;
  }

  for (int index = 0; index < count; ++index) {
    auto buffer = std::make_unique<_screen_buffer>();
    buffer->id = model().nextId++;
    buffer->owner = window;
    buffer->alive = true;
    window->renderBuffers.push_back(buffer.get());
    model().observations.allocatedBuffers.push_back(buffer.get());
    model().buffers.push_back(std::move(buffer));
    ++model().observations.liveBuffers;
  }
  return 0;
}

extern "C" int screen_fill(
    screen_context_t context,
    screen_buffer_t buffer,
    const int* attributes) {
  record(Function::Fill);
  auto call = copyFillCall(context, buffer, attributes);
  const bool attributesTerminated = call.attributesTerminated;
  model().observations.fills.push_back(std::move(call));
  if (!isAlive(context) || !isAlive(buffer)) {
    return failInvalidHandle();
  }
  if (failIfRequested(FailurePoint::Fill)) {
    return -1;
  }
  if (attributes != nullptr && !attributesTerminated) {
    errno = EINVAL;
    return -1;
  }
  return 0;
}

extern "C" int screen_post_window(
    screen_window_t window,
    screen_buffer_t buffer,
    int count,
    const int* dirtyRects,
    int flags) {
  record(Function::PostWindow);
  qnx_screen_fake::PostCall call{
      window,
      buffer,
      count,
      {},
      dirtyRects == nullptr,
      flags,
  };
  if (dirtyRects != nullptr && count > 0) {
    call.dirtyRects.assign(dirtyRects, dirtyRects + (count * 4));
  }
  model().observations.posts.push_back(std::move(call));

  if (!isAlive(window) || !isAlive(buffer) || buffer->owner != window) {
    return failInvalidHandle();
  }
  if (count < 0 || (count > 0 && dirtyRects == nullptr)) {
    errno = EINVAL;
    return -1;
  }
  if (failIfRequested(FailurePoint::PostWindow)) {
    return -1;
  }

  // Emulate the available-render-buffer list changing after a successful post.
  // QNX clients must query SCREEN_PROPERTY_RENDER_BUFFERS again before drawing.
  if (window->renderBuffers.size() > 1) {
    std::rotate(
        window->renderBuffers.begin(),
        window->renderBuffers.begin() + 1,
        window->renderBuffers.end());
  }
  return 0;
}
