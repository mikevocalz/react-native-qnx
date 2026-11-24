#pragma once

// GUI-side implementation of the Nitro windows API services
// (packages/qnx-nitro-runtime/cpp/QnxWindowServices.hpp). It owns the state
// publisher and the close gate for one window. Requests from the JavaScript
// thread reach the GUI thread through a GUI-owned QueuedCallTarget (inline
// when the caller already is on the GUI thread); results and events go back
// through the callbacks the adapter supplies. See docs/nitro/API_CONTRACTS.md,
// "Reads and threads".

#include "QnxWindowServices.hpp"
#include "WindowChromeModel.h"
#include "WindowCloseGate.h"
#include "WindowStatePublisher.h"

#include <QObject>

#include <memory>

class QQuickWindow;

namespace rnqnx::runtime {
class QueuedCallTarget;
}

namespace rnqnx::window {

class WindowChrome;

class QtWindowServices final : public QObject {
 public:
  // GUI thread. All references must outlive this object; `gui` lives on the GUI thread.
  QtWindowServices(QQuickWindow& window, WindowChrome& chrome, WindowCloseGate& gate,
                   WindowCapabilities capabilities, bool forcedFullScreen,
                   runtime::QueuedCallTarget& gui,
                   QObject* parent = nullptr);
  ~QtWindowServices() override;

  std::shared_ptr<nitro::WindowServices> services() const { return services_; }
  std::shared_ptr<WindowStatePublisher> publisher() const { return publisher_; }

  // Capability names for the API: close, minimize, maximize, move, resize.
  static std::vector<std::string> capabilityNames(const WindowCapabilities& capabilities,
                                                  bool forcedFullScreen);

 private:
  void onGui(std::function<void()> work);
  void refresh();
  void refreshInset();
  WindowState snapshot() const;
  void setBounds(nitro::Rect bounds, std::function<void(std::string, nitro::Rect)> done);
  void minimize(std::function<void(std::string)> done);

  QQuickWindow& window_;
  WindowChrome& chrome_;
  WindowCloseGate& gate_;
  WindowCapabilities capabilities_;
  runtime::QueuedCallTarget& gui_;
  std::shared_ptr<WindowStatePublisher> publisher_{std::make_shared<WindowStatePublisher>()};
  std::shared_ptr<nitro::WindowServices> services_{std::make_shared<nitro::WindowServices>()};
  nitro::WindowEventHandlers handlers_;
  bool lastVisible_{false};
  bool lastFocused_{false};
};

}  // namespace rnqnx::window
