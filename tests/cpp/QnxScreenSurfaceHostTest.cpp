#include "QnxScreenSurface.h"
#include "qnx-screen-fake/FakeQnxScreen.h"

#include <bit>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

/*
 * These host tests exercise production QnxScreenSurface code against test-only
 * emulations of the official QNX OS 8 Screen symbols. They are not QNX runtime
 * or driver tests and must never be built with the real libscreen.
 */

static_assert(SCREEN_APPLICATION_CONTEXT == 0);
static_assert(SCREEN_APPLICATION_WINDOW == 0);
static_assert(SCREEN_CHILD_WINDOW == 1);
static_assert(SCREEN_FORMAT_RGBA8888 == 8);
static_assert(SCREEN_USAGE_NATIVE == (1 << 3));
static_assert(SCREEN_PROPERTY_BUFFER_SIZE == 5);
static_assert(SCREEN_PROPERTY_FORMAT == 14);
static_assert(SCREEN_PROPERTY_RENDER_BUFFERS == 37);
static_assert(SCREEN_PROPERTY_SIZE == 40);
static_assert(SCREEN_PROPERTY_USAGE == 48);
static_assert(SCREEN_PROPERTY_RENDER_BUFFER_COUNT == 53);
static_assert(SCREEN_BLIT_END == 0);
static_assert(SCREEN_BLIT_DESTINATION_X == 5);
static_assert(SCREEN_BLIT_DESTINATION_Y == 6);
static_assert(SCREEN_BLIT_DESTINATION_WIDTH == 7);
static_assert(SCREEN_BLIT_DESTINATION_HEIGHT == 8);
static_assert(SCREEN_BLIT_COLOR == 12);

namespace {

using qnx_screen_fake::FailurePoint;
using qnx_screen_fake::Function;
using rnqnx::QnxScreenSurface;

class TestFailure final : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

[[noreturn]] void fail(
    const char* expression,
    const char* file,
    int line,
    const std::string& detail = {}) {
  std::ostringstream message;
  message << file << ':' << line << ": check failed: " << expression;
  if (!detail.empty()) {
    message << " (" << detail << ')';
  }
  throw TestFailure(message.str());
}

#define CHECK(expression)                                                \
  do {                                                                   \
    if (!(expression)) {                                                 \
      fail(#expression, __FILE__, __LINE__);                             \
    }                                                                    \
  } while (false)

template <typename Exception, typename Callable>
void expectThrows(
    Callable&& callable,
    const char* expression,
    const char* file,
    int line) {
  try {
    std::forward<Callable>(callable)();
  } catch (const Exception&) {
    return;
  } catch (const std::exception& error) {
    fail(expression, file, line, std::string("wrong exception: ") + error.what());
  } catch (...) {
    fail(expression, file, line, "wrong non-standard exception");
  }
  fail(expression, file, line, "no exception was thrown");
}

#define CHECK_THROWS_AS(expression, exceptionType)                       \
  expectThrows<exceptionType>(                                           \
      [&] { static_cast<void>(expression); }, #expression, __FILE__, __LINE__)

template <typename Callable>
void expectSystemErrorCode(
    Callable&& callable,
    int expectedCode,
    const char* expression,
    const char* file,
    int line) {
  try {
    std::forward<Callable>(callable)();
  } catch (const std::system_error& error) {
    if (error.code().value() != expectedCode) {
      fail(
          expression,
          file,
          line,
          "wrong errno: " + std::to_string(error.code().value()));
    }
    return;
  } catch (const std::exception& error) {
    fail(expression, file, line, std::string("wrong exception: ") + error.what());
  } catch (...) {
    fail(expression, file, line, "wrong non-standard exception");
  }
  fail(expression, file, line, "no exception was thrown");
}

#define CHECK_SYSTEM_ERROR(expression, errorCode)                        \
  expectSystemErrorCode(                                                 \
      [&] { static_cast<void>(expression); },                            \
      errorCode,                                                         \
      #expression,                                                       \
      __FILE__,                                                          \
      __LINE__)

void checkNoLiveResources() {
  const auto& state = qnx_screen_fake::state();
  CHECK(state.liveContexts == 0);
  CHECK(state.liveWindows == 0);
  CHECK(state.liveBuffers == 0);
  CHECK(state.invalidHandleCalls == 0);
}

void checkCall(
    const qnx_screen_fake::Call& actual,
    Function expectedFunction,
    int expectedDetail) {
  CHECK(actual.function == expectedFunction);
  CHECK(actual.detail == expectedDetail);
}

void successfulInitializationUsesExactWindowAndBufferSetup() {
  QnxScreenSurface surface;
  surface.initialize(SCREEN_CHILD_WINDOW, 1024, 600);

  const auto& state = qnx_screen_fake::state();
  CHECK(surface.context() != nullptr);
  CHECK(surface.window() != nullptr);
  CHECK(surface.width() == 1024);
  CHECK(surface.height() == 600);
  CHECK(state.liveContexts == 1);
  CHECK(state.liveWindows == 1);
  CHECK(state.liveBuffers == QnxScreenSurface::kBufferCount);

  CHECK(state.contextFlags == std::vector<int>{SCREEN_APPLICATION_CONTEXT});
  CHECK(state.windowCreations.size() == 1);
  CHECK(state.windowCreations[0].context == surface.context());
  CHECK(state.windowCreations[0].window == surface.window());
  CHECK(state.windowCreations[0].type == SCREEN_CHILD_WINDOW);
  CHECK(state.windowCreations[0].usedTypedApi);

  CHECK(state.windowPropertyWrites.size() == 4);
  CHECK(state.windowPropertyWrites[0].window == surface.window());
  CHECK(state.windowPropertyWrites[0].property == SCREEN_PROPERTY_USAGE);
  CHECK(state.windowPropertyWrites[0].values == std::vector<int>{SCREEN_USAGE_NATIVE});
  CHECK(state.windowPropertyWrites[1].property == SCREEN_PROPERTY_FORMAT);
  CHECK(state.windowPropertyWrites[1].values == std::vector<int>{SCREEN_FORMAT_RGBA8888});
  CHECK(state.windowPropertyWrites[2].property == SCREEN_PROPERTY_BUFFER_SIZE);
  CHECK(state.windowPropertyWrites[2].values == (std::vector<int>{1024, 600}));
  CHECK(state.windowPropertyWrites[3].property == SCREEN_PROPERTY_SIZE);
  CHECK(state.windowPropertyWrites[3].values == (std::vector<int>{1024, 600}));

  CHECK(state.windowBufferCreations.size() == 1);
  CHECK(state.windowBufferCreations[0].window == surface.window());
  CHECK(state.windowBufferCreations[0].count == QnxScreenSurface::kBufferCount);
  CHECK(state.windowPropertyIntReads.size() == 1);
  CHECK(state.windowPropertyIntReads[0].property == SCREEN_PROPERTY_RENDER_BUFFER_COUNT);
  CHECK(state.windowPropertyPointerReads.empty());
  CHECK(state.allocatedBuffers.size() == QnxScreenSurface::kBufferCount);

  CHECK(state.calls.size() == 8);
  checkCall(state.calls[0], Function::CreateContext, SCREEN_APPLICATION_CONTEXT);
  checkCall(state.calls[1], Function::CreateWindowType, SCREEN_CHILD_WINDOW);
  checkCall(state.calls[2], Function::SetWindowPropertyIv, SCREEN_PROPERTY_USAGE);
  checkCall(state.calls[3], Function::SetWindowPropertyIv, SCREEN_PROPERTY_FORMAT);
  checkCall(
      state.calls[4],
      Function::SetWindowPropertyIv,
      SCREEN_PROPERTY_BUFFER_SIZE);
  checkCall(state.calls[5], Function::SetWindowPropertyIv, SCREEN_PROPERTY_SIZE);
  checkCall(
      state.calls[6],
      Function::CreateWindowBuffers,
      QnxScreenSurface::kBufferCount);
  checkCall(
      state.calls[7],
      Function::GetWindowPropertyIv,
      SCREEN_PROPERTY_RENDER_BUFFER_COUNT);

  CHECK(surface.currentBuffer() == state.allocatedBuffers[0]);
  CHECK(state.windowPropertyPointerReads.size() == 1);
  CHECK(state.windowPropertyPointerReads[0].property == SCREEN_PROPERTY_RENDER_BUFFERS);
  CHECK(state.calls.size() == 9);
  checkCall(
      state.calls[8],
      Function::GetWindowPropertyPv,
      SCREEN_PROPERTY_RENDER_BUFFERS);

  const std::size_t callCount = state.calls.size();
  CHECK_THROWS_AS(
      surface.initialize(SCREEN_APPLICATION_WINDOW, 1, 1),
      std::logic_error);
  CHECK_THROWS_AS(
      surface.initialize(SCREEN_APPLICATION_WINDOW, 0, 0),
      std::invalid_argument);
  CHECK(state.calls.size() == callCount);
}

void invalidInitializationSizesAreRejectedBeforeCallingScreen() {
  const std::vector<std::pair<int, int>> invalidSizes{
      {0, 360},
      {640, 0},
      {-1, 360},
      {640, -1},
  };

  for (const auto& [width, height] : invalidSizes) {
    qnx_screen_fake::reset();
    QnxScreenSurface surface;
    CHECK_THROWS_AS(
        surface.initialize(SCREEN_APPLICATION_WINDOW, width, height),
        std::invalid_argument);
    CHECK(surface.context() == nullptr);
    CHECK(surface.window() == nullptr);
    CHECK(surface.width() == 0);
    CHECK(surface.height() == 0);
    CHECK(qnx_screen_fake::state().calls.empty());
    checkNoLiveResources();
  }
}

void fillUsesDocumentedArgbTokenValueList() {
  QnxScreenSurface surface;
  surface.initialize(SCREEN_APPLICATION_WINDOW, 320, 240);

  surface.fill(0x12, 0x34, 0x56, 0xA5);

  const auto& state = qnx_screen_fake::state();
  CHECK(state.windowPropertyPointerReads.size() == 1);
  CHECK(state.fills.size() == 1);
  CHECK(state.fills[0].context == surface.context());
  CHECK(state.fills[0].buffer == state.allocatedBuffers[0]);
  CHECK(!state.fills[0].attributesWereNull);
  CHECK(state.fills[0].attributesTerminated);

  static_assert(sizeof(int) == sizeof(std::uint32_t));
  const int argb = std::bit_cast<int>(std::uint32_t{0xA5123456u});
  const std::vector<int> expectedAttributes{
      SCREEN_BLIT_COLOR,
      argb,
      SCREEN_BLIT_DESTINATION_X,
      0,
      SCREEN_BLIT_DESTINATION_Y,
      0,
      SCREEN_BLIT_DESTINATION_WIDTH,
      320,
      SCREEN_BLIT_DESTINATION_HEIGHT,
      240,
      SCREEN_BLIT_END,
  };
  CHECK(state.fills[0].attributes == expectedAttributes);

  surface.present();
  CHECK(state.posts.size() == 1);
  CHECK(state.posts[0].buffer == state.fills[0].buffer);
  CHECK(state.windowPropertyPointerReads.size() == 1);
}

void presentUsesTheBufferAcquiredForEachFrame() {
  QnxScreenSurface surface;
  surface.initialize();

  const auto& state = qnx_screen_fake::state();
  CHECK(state.allocatedBuffers.size() == 2);
  const screen_buffer_t first = state.allocatedBuffers[0];
  const screen_buffer_t second = state.allocatedBuffers[1];

  std::size_t callOffset = state.calls.size();
  CHECK(surface.currentBuffer() == first);
  CHECK(state.calls.size() == callOffset + 1);
  checkCall(
      state.calls[callOffset],
      Function::GetWindowPropertyPv,
      SCREEN_PROPERTY_RENDER_BUFFERS);

  callOffset = state.calls.size();
  surface.present();
  CHECK(state.posts.size() == 1);
  CHECK(state.posts[0].window == surface.window());
  CHECK(state.posts[0].buffer == first);
  CHECK(state.posts[0].count == 0);
  CHECK(state.posts[0].dirtyRects.empty());
  CHECK(state.posts[0].dirtyRectsWereNull);
  CHECK(state.posts[0].flags == 0);
  CHECK(state.windowPropertyPointerReads.size() == 1);
  CHECK(state.calls.size() == callOffset + 1);
  checkCall(state.calls[callOffset], Function::PostWindow, 0);

  callOffset = state.calls.size();
  CHECK(surface.currentBuffer() == second);
  CHECK(state.calls.size() == callOffset + 1);
  checkCall(
      state.calls[callOffset],
      Function::GetWindowPropertyPv,
      SCREEN_PROPERTY_RENDER_BUFFERS);

  callOffset = state.calls.size();
  surface.present();
  CHECK(state.posts.size() == 2);
  CHECK(state.posts[1].buffer == second);
  CHECK(state.windowPropertyPointerReads.size() == 2);
  CHECK(state.calls.size() == callOffset + 1);
  checkCall(state.calls[callOffset], Function::PostWindow, 0);

  callOffset = state.calls.size();
  CHECK(surface.currentBuffer() == first);
  CHECK(state.calls.size() == callOffset + 1);
  checkCall(
      state.calls[callOffset],
      Function::GetWindowPropertyPv,
      SCREEN_PROPERTY_RENDER_BUFFERS);
}

void destroyIsOrderedCompleteAndIdempotent() {
  screen_context_t context = nullptr;
  screen_window_t window = nullptr;
  std::size_t callsBeforeDestroy = 0;
  {
    QnxScreenSurface surface;
    surface.initialize();
    context = surface.context();
    window = surface.window();
    callsBeforeDestroy = qnx_screen_fake::state().calls.size();

    surface.destroy();
    CHECK(surface.context() == nullptr);
    CHECK(surface.window() == nullptr);
    CHECK_THROWS_AS(surface.currentBuffer(), std::runtime_error);
    CHECK(surface.width() == 0);
    CHECK(surface.height() == 0);
    checkNoLiveResources();

    const auto& state = qnx_screen_fake::state();
    CHECK(state.destroyedWindows == std::vector<screen_window_t>{window});
    CHECK(state.destroyedContexts == std::vector<screen_context_t>{context});
    CHECK(state.calls.size() == callsBeforeDestroy + 2);
    checkCall(state.calls[callsBeforeDestroy], Function::DestroyWindow, 0);
    checkCall(state.calls[callsBeforeDestroy + 1], Function::DestroyContext, 0);

    surface.destroy();
    CHECK(state.calls.size() == callsBeforeDestroy + 2);
  }

  CHECK(qnx_screen_fake::state().calls.size() == callsBeforeDestroy + 2);
  checkNoLiveResources();
}

void moveConstructionAndAssignmentTransferSoleOwnership() {
  screen_context_t sourceContext = nullptr;
  screen_window_t sourceWindow = nullptr;
  screen_window_t replacedWindow = nullptr;
  {
    QnxScreenSurface source;
    source.initialize(SCREEN_APPLICATION_WINDOW, 800, 600);
    sourceContext = source.context();
    sourceWindow = source.window();
    const screen_buffer_t sourceBuffer = source.currentBuffer();

    QnxScreenSurface moved(std::move(source));
    CHECK(moved.context() == sourceContext);
    CHECK(moved.window() == sourceWindow);
    CHECK(moved.width() == 800);
    CHECK(moved.height() == 600);
    CHECK(source.context() == nullptr);
    CHECK(source.window() == nullptr);
    CHECK_THROWS_AS(source.currentBuffer(), std::runtime_error);
    CHECK(source.width() == 0);
    CHECK(source.height() == 0);

    moved.present();
    CHECK(qnx_screen_fake::state().posts.size() == 1);
    CHECK(qnx_screen_fake::state().posts[0].window == sourceWindow);
    CHECK(qnx_screen_fake::state().posts[0].buffer == sourceBuffer);
    const screen_buffer_t nextSourceBuffer = moved.currentBuffer();
    CHECK(nextSourceBuffer != sourceBuffer);

    QnxScreenSurface destination;
    destination.initialize(SCREEN_CHILD_WINDOW);
    replacedWindow = destination.window();
    CHECK(qnx_screen_fake::state().liveContexts == 2);
    CHECK(qnx_screen_fake::state().liveWindows == 2);
    CHECK(qnx_screen_fake::state().liveBuffers == 4);

    destination = std::move(moved);
    CHECK(destination.context() == sourceContext);
    CHECK(destination.window() == sourceWindow);
    CHECK(destination.width() == 800);
    CHECK(destination.height() == 600);
    CHECK(moved.context() == nullptr);
    CHECK(moved.window() == nullptr);
    CHECK_THROWS_AS(moved.currentBuffer(), std::runtime_error);
    CHECK(qnx_screen_fake::state().destroyedWindows.size() == 1);
    CHECK(qnx_screen_fake::state().destroyedWindows[0] == replacedWindow);
    CHECK(qnx_screen_fake::state().liveContexts == 1);
    CHECK(qnx_screen_fake::state().liveWindows == 1);
    CHECK(qnx_screen_fake::state().liveBuffers == 2);

    destination.present();
    CHECK(qnx_screen_fake::state().posts.size() == 2);
    CHECK(qnx_screen_fake::state().posts[1].window == sourceWindow);
    CHECK(qnx_screen_fake::state().posts[1].buffer == nextSourceBuffer);

  }

  const auto& state = qnx_screen_fake::state();
  CHECK(state.destroyedWindows.size() == 2);
  CHECK(state.destroyedContexts.size() == 2);
  CHECK(state.destroyedWindows[1] == sourceWindow);
  checkNoLiveResources();
}

void expectPartialInitializationFailure(
    FailurePoint failurePoint,
    bool contextWasCreated,
    bool windowWasCreated) {
  qnx_screen_fake::reset();
  QnxScreenSurface surface;
  qnx_screen_fake::failNext(failurePoint, EIO);

  CHECK_THROWS_AS(surface.initialize(), std::runtime_error);
  CHECK(surface.context() == nullptr);
  CHECK(surface.window() == nullptr);
  CHECK_THROWS_AS(surface.currentBuffer(), std::runtime_error);
  CHECK(surface.width() == 0);
  CHECK(surface.height() == 0);
  checkNoLiveResources();

  const auto& failedState = qnx_screen_fake::state();
  CHECK(failedState.destroyedContexts.size() == (contextWasCreated ? 1u : 0u));
  CHECK(failedState.destroyedWindows.size() == (windowWasCreated ? 1u : 0u));

  // A failed initialization must leave the same object reusable.
  surface.initialize();
  CHECK(surface.context() != nullptr);
  CHECK(surface.window() != nullptr);
  surface.destroy();
  checkNoLiveResources();
}

void everyPartialInitializationFailureReleasesFakeResources() {
  expectPartialInitializationFailure(FailurePoint::CreateContext, false, false);
  expectPartialInitializationFailure(FailurePoint::CreateWindowType, true, false);
  expectPartialInitializationFailure(FailurePoint::SetWindowUsage, true, true);
  expectPartialInitializationFailure(FailurePoint::SetWindowFormat, true, true);
  expectPartialInitializationFailure(FailurePoint::SetWindowBufferSize, true, true);
  expectPartialInitializationFailure(FailurePoint::SetWindowSize, true, true);
  expectPartialInitializationFailure(FailurePoint::CreateWindowBuffers, true, true);
  expectPartialInitializationFailure(FailurePoint::GetRenderBufferCount, true, true);

  qnx_screen_fake::reset();
  QnxScreenSurface surface;
  qnx_screen_fake::reportRenderBufferCount(QnxScreenSurface::kBufferCount - 1);
  CHECK_THROWS_AS(surface.initialize(), std::runtime_error);
  CHECK(surface.context() == nullptr);
  CHECK(surface.window() == nullptr);
  CHECK_THROWS_AS(surface.currentBuffer(), std::runtime_error);
  CHECK(qnx_screen_fake::state().destroyedWindows.size() == 1);
  CHECK(qnx_screen_fake::state().destroyedContexts.size() == 1);
  checkNoLiveResources();
}

void runtimeFailuresAreExplicitAndDoNotPostFailedFills() {
  QnxScreenSurface uninitialized;
  CHECK_THROWS_AS(uninitialized.fill(1, 2, 3), std::runtime_error);
  CHECK_THROWS_AS(uninitialized.present(), std::runtime_error);
  CHECK(qnx_screen_fake::state().calls.empty());

  QnxScreenSurface surface;
  surface.initialize();
  const auto& state = qnx_screen_fake::state();

  qnx_screen_fake::failNext(FailurePoint::GetRenderBuffers, EAGAIN);
  CHECK_SYSTEM_ERROR(surface.currentBuffer(), EAGAIN);
  CHECK(state.windowPropertyPointerReads.size() == 1);

  qnx_screen_fake::returnNullRenderBufferOnce();
  CHECK_THROWS_AS(surface.currentBuffer(), std::runtime_error);
  CHECK(state.windowPropertyPointerReads.size() == 2);

  CHECK(surface.currentBuffer() == state.allocatedBuffers[0]);
  CHECK(state.windowPropertyPointerReads.size() == 3);

  qnx_screen_fake::failNext(FailurePoint::Fill, EIO);
  CHECK_SYSTEM_ERROR(surface.fill(0x12, 0x34, 0x56), EIO);
  CHECK(state.fills.size() == 1);
  CHECK(state.posts.empty());

  const std::size_t readsAfterFailedFill = state.windowPropertyPointerReads.size();
  surface.fill(0x12, 0x34, 0x56);
  CHECK(state.windowPropertyPointerReads.size() == readsAfterFailedFill + 1);
  const screen_buffer_t renderedBuffer = state.fills.back().buffer;

  qnx_screen_fake::failNext(FailurePoint::PostWindow, EBUSY);
  CHECK_SYSTEM_ERROR(surface.present(), EBUSY);
  CHECK(state.posts.size() == 1);
  CHECK(state.posts[0].buffer == renderedBuffer);

  const std::size_t readsAfterFailedPost = state.windowPropertyPointerReads.size();
  surface.present();
  CHECK(state.posts.size() == 2);
  CHECK(state.posts[1].buffer == renderedBuffer);
  CHECK(state.windowPropertyPointerReads.size() == readsAfterFailedPost);

  const std::size_t readsBeforeAutoAcquire = state.windowPropertyPointerReads.size();
  surface.present();
  CHECK(state.posts.size() == 3);
  CHECK(state.windowPropertyPointerReads.size() == readsBeforeAutoAcquire + 1);
}

struct TestCase {
  const char* name;
  void (*run)();
};

} // namespace

int main() {
  const std::vector<TestCase> tests{
      {"successful initialization uses exact window and buffer setup",
       successfulInitializationUsesExactWindowAndBufferSetup},
      {"invalid initialization sizes are rejected before calling Screen",
       invalidInitializationSizesAreRejectedBeforeCallingScreen},
      {"fill uses documented ARGB token-value list",
       fillUsesDocumentedArgbTokenValueList},
      {"present uses the buffer acquired for each frame",
       presentUsesTheBufferAcquiredForEachFrame},
      {"destroy is ordered, complete, and idempotent",
       destroyIsOrderedCompleteAndIdempotent},
      {"move construction and assignment transfer sole ownership",
       moveConstructionAndAssignmentTransferSoleOwnership},
      {"partial initialization failures release fake resources",
       everyPartialInitializationFailureReleasesFakeResources},
      {"runtime failures are explicit and failed fills are not posted",
       runtimeFailuresAreExplicitAndDoNotPostFailedFills},
  };

  int failed = 0;
  for (const auto& test : tests) {
    qnx_screen_fake::reset();
    try {
      test.run();
      checkNoLiveResources();
      std::cout << "[PASS] " << test.name << '\n';
    } catch (const std::exception& error) {
      ++failed;
      std::cerr << "[FAIL] " << test.name << ": " << error.what();
      const auto& state = qnx_screen_fake::state();
      if (state.liveContexts != 0 || state.liveWindows != 0 ||
          state.liveBuffers != 0) {
        std::cerr << " (live contexts/windows/buffers: " << state.liveContexts << '/'
                  << state.liveWindows << '/' << state.liveBuffers << ')';
      }
      std::cerr << '\n';
    } catch (...) {
      ++failed;
      std::cerr << "[FAIL] " << test.name << ": unknown exception\n";
    }
  }

  if (failed != 0) {
    std::cerr << failed << " QnxScreenSurface host test(s) failed.\n";
    return EXIT_FAILURE;
  }

  std::cout << "All QnxScreenSurface host tests passed.\n";
  return EXIT_SUCCESS;
}
