// Stage 5 test app for react-native-qnx: proves a Dawn WebGPU device on QNX.
//
//   rnqnx_webgpu_test [--backend=opengles|null] [--headless] [--out-dir=DIR]
//                     [--hold-seconds=N] [--skip-loss]
//
// Headless stages: adapter/limits, compute, offscreen clear+triangle+readback.
// Window stages: present to a Screen window, resize, surface and window
// recreation, injected device loss and recovery, then a hold for a screenshot.
// The exit status is 0 only when every stage that ran passed.
#include <chrono>
#include <csignal>
#include <exception>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <cstring>
#include <string>
#include <thread>

#include "Check.h"
#include "GpuContext.h"
#include "ImageWriter.h"
#include "Log.h"
#include "Scenarios.h"
#include "ScreenWindow.h"
#include "SurfacePresenter.h"

using namespace rnqnx::webgpu;

namespace {

struct Options {
  BackendChoice backend = BackendChoice::OpenGLES;
  // The null backend executes no GPU work, so pixel and value checks only make
  // sense on a real backend.
  bool verifyValues() const { return backend != BackendChoice::Null; }
  bool headless = false;
  bool skipLoss = false;
  int holdSeconds = 0;
  std::string outDir = ".";
};

bool startsWith(const char* text, const char* prefix) {
  return std::strncmp(text, prefix, std::strlen(prefix)) == 0;
}

bool parseArgs(int argc, char** argv, Options& options) {
  for (int i = 1; i < argc; ++i) {
    const char* arg = argv[i];
    if (startsWith(arg, "--backend=")) {
      const char* value = arg + 10;
      if (!std::strcmp(value, "opengles")) options.backend = BackendChoice::OpenGLES;
      else if (!std::strcmp(value, "null")) options.backend = BackendChoice::Null;
      else return false;
    } else if (!std::strcmp(arg, "--headless")) {
      options.headless = true;
    } else if (!std::strcmp(arg, "--skip-loss")) {
      options.skipLoss = true;
    } else if (startsWith(arg, "--hold-seconds=")) {
      options.holdSeconds = std::atoi(arg + 15);
    } else if (startsWith(arg, "--out-dir=")) {
      options.outDir = arg + 10;
    } else {
      return false;
    }
  }
  return true;
}

bool near(Rgba8 got, Rgba8 want, int tolerance = 3) {
  auto d = [](int a, int b) { return a > b ? a - b : b - a; };
  return d(got.r, want.r) <= tolerance && d(got.g, want.g) <= tolerance &&
         d(got.b, want.b) <= tolerance && d(got.a, want.a) <= tolerance;
}

std::string describe(Rgba8 p) {
  return "(" + std::to_string(p.r) + "," + std::to_string(p.g) + "," + std::to_string(p.b) + "," +
         std::to_string(p.a) + ")";
}

// Checks the corner holds the clear color and the center the triangle's red.
void checkFrame(Report& report, const Image& image, Rgba8 clear, bool triangle, const char* what) {
  const Rgba8 corner = image.at(2, 2);
  if (!near(corner, clear)) {
    report.fail(std::string(what) + ": corner " + describe(corner) + " != clear " + describe(clear));
    return;
  }
  const Rgba8 center = image.at(image.width / 2, image.height / 2);
  const Rgba8 want = triangle ? Rgba8{255, 0, 0, 255} : clear;
  if (!near(center, want)) {
    report.fail(std::string(what) + ": center " + describe(center) + " != " + describe(want));
    return;
  }
  report.note(std::string(what) + " corner=" + describe(corner) + " center=" + describe(center));
}

constexpr wgpu::Color kBlue{0.0, 0.0, 1.0, 1.0};
constexpr Rgba8 kBlue8{0, 0, 255, 255};
constexpr wgpu::Color kGreen{0.0, 0.5, 0.0, 1.0};
constexpr Rgba8 kGreen8{0, 128, 0, 255};

// Drains and reports Dawn's uncaptured errors; fails the open stage if any.
void expectNoUncapturedErrors(GpuContext& gpu, Report& report, const char* where) {
  const int count = gpu.takeUncapturedErrors();
  if (count != 0) report.fail(std::to_string(count) + " uncaptured errors " + where);
}

void headlessStages(GpuContext& gpu, Report& report, const Options& options) {
  std::string error;
  report.begin("offscreen-render-readback");
  if (!options.verifyValues()) {
    report.note("null backend: calls succeeded, pixels are not checked");
  }
  constexpr uint32_t kSize = 256;
  wgpu::TextureDescriptor descriptor{};
  descriptor.size = {kSize, kSize, 1};
  descriptor.format = wgpu::TextureFormat::RGBA8Unorm;
  descriptor.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc;
  wgpu::Texture target = gpu.device().CreateTexture(&descriptor);
  TriangleRenderer renderer;
  if (!renderer.init(gpu.device(), descriptor.format, error)) {
    report.fail(error);
  } else {
    Image image;
    renderer.encode(gpu.device(), gpu.queue(), target.CreateView(), kBlue, false);
    if (!readbackTexture(gpu, target, kSize, kSize, false, image, error)) report.fail(error);
    else if (options.verifyValues()) checkFrame(report, image, kBlue8, false, "clear");
    renderer.encode(gpu.device(), gpu.queue(), target.CreateView(), kGreen, true);
    if (!readbackTexture(gpu, target, kSize, kSize, false, image, error)) report.fail(error);
    else {
      if (options.verifyValues()) checkFrame(report, image, kGreen8, true, "triangle");
      std::string writeError;
      if (!writeBmp(options.outDir + "/offscreen-triangle.bmp", image, writeError)) {
        logLine("NOTE", "bmp not written: %s", writeError.c_str());
      }
    }
  }
  expectNoUncapturedErrors(gpu, report, "in the offscreen render");
  report.end();

  report.begin("compute");
  if (!runComputeDouble(gpu, 4096, options.verifyValues(), error)) report.fail(error);
  else report.note(options.verifyValues() ? "4096 u32 doubled and verified" : "compute dispatched (values not verified on this backend)");
  expectNoUncapturedErrors(gpu, report, "in compute");
  report.end();
}

bool frameFits(Report& report, const Image& image, int wantW, int wantH, const ScreenWindow& window,
               const char* what) {
  int windowSize[2] = {0, 0};
  int bufferSize[2] = {0, 0};
  if (!window.querySizes(windowSize, bufferSize)) {
    report.fail(std::string(what) + ": could not query the Screen window size");
    return false;
  }
  if (image.width != static_cast<uint32_t>(wantW) || image.height != static_cast<uint32_t>(wantH) ||
      windowSize[0] != wantW || windowSize[1] != wantH || bufferSize[0] != wantW ||
      bufferSize[1] != wantH) {
    report.fail(std::string(what) + ": texture " + std::to_string(image.width) + "x" +
                std::to_string(image.height) + ", window " + std::to_string(windowSize[0]) + "x" +
                std::to_string(windowSize[1]) + ", buffers " + std::to_string(bufferSize[0]) + "x" +
                std::to_string(bufferSize[1]) + ", wanted " + std::to_string(wantW) + "x" +
                std::to_string(wantH));
    return false;
  }
  return true;
}

// Returns false when the run cannot continue (the surface or window is gone).
bool windowStages(GpuContext& gpu, Report& report, const Options& options) {
  std::string error;
  Image image;
  ScreenWindow::Options windowOptions;

  report.begin("window-and-surface-create");
  auto window = ScreenWindow::create(windowOptions, error);
  if (!window) {
    report.fail(error);
    report.end();
    return false;
  }
  auto presenter = std::make_unique<SurfacePresenter>(gpu, *window);
  if (!presenter->create(error)) {
    report.fail(error);
    report.end();
    return false;
  }
  report.note("window " + std::to_string(window->width()) + "x" + std::to_string(window->height()));
  report.end();

  // "swapchain" stages: Dawn's OpenGL ES swap chain renders into its own
  // texture and blits it to the EGL window surface in Present(). The readback
  // below therefore proves the swap chain texture; the on-glass screenshot taken
  // by the launcher proves the window.
  report.begin("render-to-swapchain-texture");
  if (!presenter->drawFrame(kBlue, false, image, error)) report.fail(error);
  else checkFrame(report, image, kBlue8, false, "surface clear");
  if (!presenter->drawFrame(kGreen, true, image, error)) report.fail(error);
  else {
    checkFrame(report, image, kGreen8, true, "surface triangle");
    std::string writeError;
    writeBmp(options.outDir + "/surface-triangle.bmp", image, writeError);
  }
  expectNoUncapturedErrors(gpu, report, "while presenting");
  report.end();

  report.begin("resize");
  const int sizes[][2] = {{800, 600}, {480, 360}, {640, 480}};
  for (const auto& size : sizes) {
    if (!presenter->resize(size[0], size[1], error) ||
        !presenter->drawFrame(kGreen, true, image, error)) {
      report.fail(error);
      break;
    }
    if (!frameFits(report, image, size[0], size[1], *window, "after resize")) break;
    checkFrame(report, image, kGreen8, true, "resized frame");
  }
  expectNoUncapturedErrors(gpu, report, "while resizing");
  report.end();

  report.begin("surface-recreate");
  presenter->dropSurface();
  if (!presenter->create(error)) report.fail(error);
  else if (!presenter->drawFrame(kBlue, true, image, error)) report.fail(error);
  else if (frameFits(report, image, 640, 480, *window, "recreated surface")) {
    checkFrame(report, image, kBlue8, true, "recreated surface");
  }
  expectNoUncapturedErrors(gpu, report, "while recreating the surface");
  report.end();

  report.begin("window-recreate");
  presenter.reset();
  window.reset();
  window = ScreenWindow::create(windowOptions, error);
  if (!window) {
    report.fail(error);
    report.end();
    return false;
  }
  presenter = std::make_unique<SurfacePresenter>(gpu, *window);
  if (!presenter->create(error)) report.fail(error);
  else if (!presenter->drawFrame(kGreen, true, image, error)) report.fail(error);
  else if (frameFits(report, image, 640, 480, *window, "recreated window")) {
    checkFrame(report, image, kGreen8, true, "recreated window");
  }
  expectNoUncapturedErrors(gpu, report, "while recreating the window");
  report.end();
  if (!presenter->valid()) return false;

  if (!options.skipLoss) {
    report.begin("device-loss-recovery-injected");
    // The loss is injected with Device::ForceLoss. Patch 0003 means the driver
    // cannot report a real GPU reset on this Mesa, so a real one is not tested.
    presenter->releaseDeviceObjects();
    if (!gpu.injectDeviceLoss()) {
      report.fail("the injected device loss was not reported");
    } else {
      report.note("loss reported: " + gpu.lossReason());
      // Anything Dawn raised because of the loss itself is counted and logged, not failed.
      const int lossErrors = gpu.takeUncapturedErrors();
      logLine("DEVICE", "uncaptured_errors_during_loss=%d", lossErrors);
      if (!gpu.recreateDevice(error)) {
        report.fail(error);
      } else if (!presenter->create(error)) {
        report.fail(error);
      } else if (!presenter->drawFrame(kBlue, true, image, error)) {
        report.fail(error);
      } else {
        checkFrame(report, image, kBlue8, true, "after recovery");
        std::string computeError;
        if (!runComputeDouble(gpu, 1024, options.verifyValues(), computeError)) {
          report.fail(computeError);
        } else {
          report.note("compute works on the new device");
        }
      }
      expectNoUncapturedErrors(gpu, report, "after recovery");
    }
    report.end();
    if (!presenter->valid()) return false;
  }

  // Final visible frame, held for the launcher's screenshot. The ready marker is
  // written only when everything so far passed and this frame rendered.
  if (!presenter->drawFrame(kGreen, true, image, error)) {
    logLine("HOLD", "final frame failed: %s", error.c_str());
    return false;
  }
  std::string writeError;
  writeBmp(options.outDir + "/final-surface.bmp", image, writeError);
  bool holdOk = true;
  if (options.holdSeconds > 0 && report.allPassed()) {
    std::FILE* ready = std::fopen((options.outDir + "/ready").c_str(), "w");
    if (ready) std::fclose(ready);
    logLine("HOLD", "seconds=%d", options.holdSeconds);
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(options.holdSeconds);
    int frame = 0;
    while (std::chrono::steady_clock::now() < until) {
      if (!window->pumpEvents()) {
        logLine("HOLD", "close requested");
        break;
      }
      // Animate the clear colour so a stuck compositor would be visible.
      const double t = (frame++ % 120) / 120.0;
      if (!presenter->drawFrame({0.1, 0.2 + 0.3 * t, 0.5, 1.0}, true, image, error)) {
        logLine("HOLD", "frame failed: %s", error.c_str());
        holdOk = false;
        break;
      }
    }
  }
  if (!holdOk) {
    report.begin("hold-frames");
    report.fail("a frame failed while the window was held for the screenshot");
    report.end();
  }
  return true;
}

}  // namespace

// A crash inside the driver or Dawn should leave a line in the log, because the
// launcher only keeps the app's output.
static void reportFatalSignal(int signal) {
  static const char kPrefix[] = "RNQNX_WEBGPU FATAL signal ";
  char text[64];
  int length = 0;
  for (const char* c = kPrefix; *c; ++c) text[length++] = *c;
  if (signal >= 10) text[length++] = static_cast<char>('0' + signal / 10);
  text[length++] = static_cast<char>('0' + signal % 10);
  text[length++] = '\n';
  const ssize_t written = write(STDOUT_FILENO, text, static_cast<size_t>(length));
  (void)written;
  _exit(128 + signal);
}

int main(int argc, char** argv) {
  std::signal(SIGABRT, reportFatalSignal);
  std::signal(SIGSEGV, reportFatalSignal);
  std::signal(SIGBUS, reportFatalSignal);
  std::signal(SIGFPE, reportFatalSignal);
  std::signal(SIGILL, reportFatalSignal);
  std::set_terminate([]() {
    logLine("FATAL", "std::terminate");
    _exit(134);
  });
  Options options;
  if (!parseArgs(argc, argv, options)) {
    std::fprintf(stderr,
                 "usage: %s [--backend=opengles|null] [--headless] [--skip-loss] "
                 "[--hold-seconds=N] [--out-dir=DIR]\n",
                 argv[0]);
    return 2;
  }

  Report report;
  report.begin("adapter-device");
  std::string error;
  auto gpu = GpuContext::create(options.backend, error);
  if (!gpu) {
    report.fail(error);
    report.end();
    logLine("RESULT", "FAIL stages=%zu", report.stages().size());
    return 1;
  }
  gpu->logCapabilities();
  report.note("adapter and device created");
  report.end();

  headlessStages(*gpu, report, options);
  if (!options.headless && options.backend == BackendChoice::OpenGLES) {
    if (!windowStages(*gpu, report, options)) {
      report.begin("window-run-completed");
      report.fail("the window stages could not run to the end");
      report.end();
    }
  }

  const int uncaptured = gpu->takeUncapturedErrors();
  report.begin("no-uncaptured-errors");
  if (uncaptured != 0) report.fail(std::to_string(uncaptured) + " uncaptured errors");
  report.end();

  int passed = 0;
  for (const auto& stage : report.stages()) passed += stage.passed ? 1 : 0;
  // verified=0 means the backend executes no GPU work (null), so PASS covers
  // API validation only, not pixels or compute values.
  logLine("RESULT", "%s stages=%zu passed=%d verified=%d", report.allPassed() ? "PASS" : "FAIL",
          report.stages().size(), passed, options.verifyValues() ? 1 : 0);
  return report.allPassed() ? 0 : 1;
}
