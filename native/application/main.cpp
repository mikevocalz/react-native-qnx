// Minimal QNX Screen application entry point for Milestone 0.
//
// Opens a QNX Screen window, fills the back buffer with a solid color, presents
// it, and keeps the window alive until interrupted. This validates the toolchain,
// Screen library linkage, and basic resource lifecycle.

#include "QnxScreenSurface.h"

#include <screen/screen.h>
#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <thread>
#include <chrono>

namespace {

std::sig_atomic_t g_running = 1;

void signalHandler(int /*signal*/) {
  g_running = 0;
}

} // namespace

int main(int argc, char* argv[]) {
  std::signal(SIGINT, signalHandler);
  std::signal(SIGTERM, signalHandler);

  (void)argc;
  (void)argv;

  try {
    rnqnx::QnxScreenSurface surface;
    surface.initialize(SCREEN_WINDOW_TYPE_APPLICATION);

    // Use a modest default window size. The actual display size should be queried
    // from Screen properties in a real application.
    surface.setSize(1280, 720);

    std::printf("rnqnx_playground: Screen surface initialized (%dx%d)\n",
                surface.width(), surface.height());

    // Render a few frames with alternating colors so the window is visible.
    int frame = 0;
    while (g_running && frame < 600) {
      if (frame % 2 == 0) {
        surface.fill(0x29, 0x6E, 0xD8); // React blue-ish
      } else {
        surface.fill(0x00, 0x00, 0x00); // black
      }
      surface.present();
      ++frame;
      std::this_thread::sleep_for(std::chrono::milliseconds(33));
    }

    std::printf("rnqnx_playground: exiting cleanly after %d frames\n", frame);
    return EXIT_SUCCESS;
  } catch (const std::exception& e) {
    std::fprintf(stderr, "rnqnx_playground fatal error: %s\n", e.what());
    return EXIT_FAILURE;
  }
}
