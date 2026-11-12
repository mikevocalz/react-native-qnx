// Compiled once per fixture with the header CMake generated from it.
#include RNQNX_CONSUMER_WINDOW_HEADER

#include <cstdio>
#include <cstring>

int main() {
  using namespace rnqnx::window;
  const WindowChromeConfig config = configuredWindowChrome();
  const char* name = RNQNX_EXPECTED_CASE;
  bool ok = false;
  if (std::strcmp(name, "windows-hidden-sized") == 0) {
    ok = config.controls == ControlsLayout::Windows && config.titleBarStyle == TitleBarStyle::Hidden &&
         config.initialSize == QSize(1024, 600);
  } else if (std::strcmp(name, "legacy-no-window") == 0) {
    // Descriptors written before the field keep the chromeless window.
    ok = config.titleBarStyle == TitleBarStyle::None && !config.initialSize;
  } else if (std::strcmp(name, "empty-window") == 0) {
    ok = config.controls == ControlsLayout::MacOS && config.titleBarStyle == TitleBarStyle::Default &&
         !config.initialSize;
  }
  std::printf("%s %s\n", ok ? "PASS" : "FAIL", name);
  return ok ? 0 : 1;
}
