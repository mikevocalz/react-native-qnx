#pragma once

// Structs of the QnxWindows spec (specs/QnxWindows.nitro.ts). Field names and
// order follow the spec; the generated binding converts them by name and the
// generated static_asserts fail the build when a signature drifts.

#include <string>

namespace rnqnx::nitro {

struct Rect {
  double x{0};
  double y{0};
  double width{0};
  double height{0};
};

struct TitleBarInsetData {
  double top{0};
  double right{0};
  double bottom{0};
  double left{0};
  double overlayX{0};
  double overlayY{0};
  double overlayWidth{0};
  double overlayHeight{0};
};

struct WindowStateData {
  double sequence{0};
  std::string title;
  double width{0};
  double height{0};
  double x{0};
  double y{0};
  bool focused{false};
  bool visible{false};
  bool maximized{false};
  std::string controlsLayout;
};

}  // namespace rnqnx::nitro
