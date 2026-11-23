#include "QnxWindows.hpp"

#include <stdexcept>
#include <utility>

namespace rnqnx::nitro {

std::shared_ptr<QnxWindows> QnxWindows::create() {
  if (!QnxWindowsBackend::active()) {
    throw std::runtime_error(
        "WINDOWS_UNAVAILABLE: the host installed no window services (built without "
        "RNQNX_ENABLE_WINDOW_CHROME, or not a QNX Qt host)");
  }
  return std::make_shared<QnxWindows>();
}

std::shared_ptr<QnxWindowsBackend> QnxWindows::backend() const {
  auto backend = QnxWindowsBackend::active();
  if (!backend) throw std::runtime_error("WINDOWS_DISPOSED: the JavaScript runtime was torn down");
  return backend;
}

std::vector<std::string> QnxWindows::getCapabilities() { return backend()->capabilities(); }
WindowStateData QnxWindows::getState() { return backend()->state(); }
margelo::nitro::Promise<void> QnxWindows::setControlsLayout(std::string layout) {
  return backend()->setControlsLayout(layout);
}
margelo::nitro::Promise<void> QnxWindows::setTitle(std::string title) {
  return backend()->setTitle(title);
}
margelo::nitro::Promise<Rect> QnxWindows::setBounds(Rect bounds) {
  return backend()->setBounds(bounds);
}
margelo::nitro::Promise<void> QnxWindows::minimize() { return backend()->minimize(); }
margelo::nitro::Promise<bool> QnxWindows::close(bool force) { return backend()->close(force); }
double QnxWindows::addCloseGuard(margelo::nitro::Callback<void(double, double, std::string, bool, double)> guard) {
  return static_cast<double>(backend()->addCloseGuard(std::move(guard)));
}
void QnxWindows::resolveCloseGuard(double requestId, double guardId, bool allow) {
  backend()->resolveCloseGuard(static_cast<std::uint64_t>(requestId),
                               static_cast<std::uint64_t>(guardId), allow);
}
double QnxWindows::addOnFocusChangedListener(margelo::nitro::Callback<void(bool)> listener) {
  return static_cast<double>(backend()->addFocusListener(std::move(listener)));
}
double QnxWindows::addOnVisibilityChangedListener(margelo::nitro::Callback<void(bool)> listener) {
  return static_cast<double>(backend()->addVisibilityListener(std::move(listener)));
}
TitleBarInsetData QnxWindows::getTitleBarInset() { return backend()->titleBarInset(); }
double QnxWindows::addOnInsetChangedListener(
    margelo::nitro::Callback<void(double, double, double, double, double, double, double, double)> listener) {
  return static_cast<double>(backend()->addInsetListener(std::move(listener)));
}
void QnxWindows::removeListener(double id) {
  // After teardown there is nothing to remove; the call is idempotent.
  if (auto backend = QnxWindowsBackend::active()) backend->removeListener(static_cast<std::uint64_t>(id));
}

}  // namespace rnqnx::nitro
