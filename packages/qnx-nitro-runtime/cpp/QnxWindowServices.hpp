#pragma once

// What the Nitro windows adapter needs from the GUI side, injected at install
// time. No Qt here. The GUI-side implementation (native/window-chrome/
// QtWindowServices) runs every request on the GUI thread, inline when the
// caller is already there, and calls `done` and the event handlers on the GUI
// thread. The adapter posts anything that touches JavaScript back through the
// Nitro dispatcher (QtRuntimeTaskQueue::post).
//
// Errors are `CODE: reason` strings; empty means success.

#include "QnxWindowsTypes.hpp"
#include "WindowCloseRequest.h"
#include "WindowStatePublisher.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace rnqnx::nitro {

struct WindowEventHandlers {
  std::function<void(const window::CloseRequestInfo&)> onCloseRequest;
  std::function<void(bool focused)> onFocusChanged;
  std::function<void(bool visible)> onVisibilityChanged;
  // The inset changed. The adapter coalesces through the publisher's pending flag.
  std::function<void()> onInsetChanged;
};

struct WindowServices {
  // Final once the window manager has answered; read before JavaScript loads.
  std::vector<std::string> capabilities;
  std::shared_ptr<window::WindowStatePublisher> state;
  // Shared with the close gate. The adapter bumps it on teardown.
  std::shared_ptr<std::atomic<std::uint64_t>> epoch;

  std::function<void(std::string title, std::function<void(std::string error)> done)> setTitle;
  std::function<void(std::string layout, std::function<void(std::string error)> done)>
      setControlsLayout;
  std::function<void(Rect bounds, std::function<void(std::string error, Rect applied)> done)>
      setBounds;
  std::function<void(std::function<void(std::string error)> done)> minimize;
  std::function<void(bool force, std::function<void(bool accepted)> done)> requestClose;

  std::function<void(std::uint64_t guardId)> guardAdded;
  std::function<void(std::uint64_t guardId)> guardRemoved;
  std::function<void(std::uint64_t requestId, std::uint64_t epoch, bool allow)> settleClose;
  // The JavaScript runtime is gone: forget the guard ids; a pending close proceeds.
  std::function<void()> runtimeGone;
  // Connects the GUI-to-JS events. Called again for each runtime; a later call replaces handlers.
  std::function<void(WindowEventHandlers handlers)> connect;
};

}  // namespace rnqnx::nitro
