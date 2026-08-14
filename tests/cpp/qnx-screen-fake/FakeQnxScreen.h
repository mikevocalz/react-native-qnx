#pragma once

#include <screen/screen.h>

#include <cstddef>
#include <vector>

/*
 * Test-only control and observation surface for the emulated QNX OS 8 Screen
 * symbols declared by screen/screen.h beside this file. Production code must
 * never include this header.
 */
namespace qnx_screen_fake {

enum class Function {
  CreateContext,
  DestroyContext,
  CreateWindowType,
  DestroyWindow,
  SetWindowPropertyIv,
  GetWindowPropertyIv,
  GetWindowPropertyPv,
  CreateWindowBuffers,
  Fill,
  PostWindow,
};

enum class FailurePoint {
  CreateContext,
  CreateWindowType,
  SetWindowUsage,
  SetWindowFormat,
  SetWindowBufferSize,
  SetWindowSize,
  CreateWindowBuffers,
  GetRenderBufferCount,
  GetRenderBuffers,
  Fill,
  PostWindow,
};

struct Call {
  Function function;
  int detail;
};

struct WindowCreation {
  screen_window_t window;
  screen_context_t context;
  int type;
  bool usedTypedApi;
};

struct WindowPropertyCall {
  screen_window_t window;
  int property;
  std::vector<int> values;
};

struct WindowBuffersCall {
  screen_window_t window;
  int count;
};

struct WindowPropertyRead {
  screen_window_t window;
  int property;
};

struct FillCall {
  screen_context_t context;
  screen_buffer_t buffer;
  std::vector<int> attributes;
  bool attributesTerminated;
  bool attributesWereNull;
};

struct PostCall {
  screen_window_t window;
  screen_buffer_t buffer;
  int count;
  std::vector<int> dirtyRects;
  bool dirtyRectsWereNull;
  int flags;
};

struct State {
  std::vector<Call> calls;
  std::vector<int> contextFlags;
  std::vector<WindowCreation> windowCreations;
  std::vector<WindowPropertyCall> windowPropertyWrites;
  std::vector<WindowPropertyRead> windowPropertyIntReads;
  std::vector<WindowPropertyRead> windowPropertyPointerReads;
  std::vector<WindowBuffersCall> windowBufferCreations;
  std::vector<FillCall> fills;
  std::vector<PostCall> posts;
  std::vector<screen_context_t> destroyedContexts;
  std::vector<screen_window_t> destroyedWindows;
  std::vector<screen_buffer_t> allocatedBuffers;
  std::size_t liveContexts = 0;
  std::size_t liveWindows = 0;
  std::size_t liveBuffers = 0;
  std::size_t invalidHandleCalls = 0;
};

/** Reset all fake objects, observations, and failure injection. */
void reset();

/** Make the next matching test-only Screen call return -1 and set errno. */
void failNext(FailurePoint point, int errorNumber);

/** Override the next reported SCREEN_PROPERTY_RENDER_BUFFER_COUNT value. */
void reportRenderBufferCount(int count);

/** Return a null first render buffer on the next successful pointer-property read. */
void returnNullRenderBufferOnce();

/** Return immutable observations for the current test. */
const State& state();

} // namespace qnx_screen_fake
