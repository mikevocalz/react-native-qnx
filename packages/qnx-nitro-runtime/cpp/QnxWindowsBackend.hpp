#pragma once

// The JavaScript-thread half of the windows API: the guard and listener
// registries (the only owners of JavaScript callbacks), the guard sequencer
// and the epoch rule. One backend per runtime. Every method runs on the
// JavaScript thread unless noted.

#include "CloseGuardSequencer.hpp"
#include "QnxWindowServices.hpp"
#include "QnxWindowsTypes.hpp"

#include <NitroModules/Callback.hpp>
#include <NitroModules/Dispatcher.hpp>
#include <NitroModules/Promise.hpp>

#include <cstdint>
#include <memory>
#include <vector>

namespace rnqnx::nitro {

class QnxWindowsBackend final : public std::enable_shared_from_this<QnxWindowsBackend> {
 public:
  using GuardCallback =
      margelo::nitro::Callback<void(double, double, std::string, bool, double)>;
  using BoolCallback = margelo::nitro::Callback<void(bool)>;
  using InsetCallback = margelo::nitro::Callback<void(double, double, double, double, double, double, double, double)>;

  // Connects the GUI events. Use create(); the handlers hold weak references.
  static std::shared_ptr<QnxWindowsBackend> create(
      std::shared_ptr<margelo::nitro::Dispatcher> dispatcher,
      std::shared_ptr<WindowServices> services);

  // The backend of the installed runtime, or null before install and after teardown.
  static std::shared_ptr<QnxWindowsBackend> active();

  // Runtime teardown hook (QtReactHost::addTeardownHook): clears both
  // registries, bumps the epoch and tells the GUI side. Posts, never waits.
  void teardown();
  bool disposed() const { return disposed_; }

  const std::vector<std::string>& capabilities() const { return services_->capabilities; }
  WindowStateData state() const;

  margelo::nitro::Promise<void> setTitle(const std::string& title);
  margelo::nitro::Promise<void> setControlsLayout(const std::string& layout);
  margelo::nitro::Promise<Rect> setBounds(const Rect& bounds);
  margelo::nitro::Promise<void> minimize();
  margelo::nitro::Promise<bool> close(bool force);

  std::uint64_t addCloseGuard(GuardCallback guard);
  void resolveCloseGuard(std::uint64_t requestId, std::uint64_t guardId, bool allow);
  std::uint64_t addFocusListener(BoolCallback listener);
  std::uint64_t addVisibilityListener(BoolCallback listener);
  TitleBarInsetData titleBarInset() const;
  std::uint64_t addInsetListener(InsetCallback listener);
  void removeListener(std::uint64_t id);

 private:
  QnxWindowsBackend(std::shared_ptr<margelo::nitro::Dispatcher> dispatcher,
                    std::shared_ptr<WindowServices> services);
  void connect();
  void requireLive() const;
  void requireCapability(const char* capability, const char* what) const;
  void postToJs(std::function<void()> task) const;
  void emitBool(bool value, std::uint64_t epoch, bool focus);

  std::shared_ptr<margelo::nitro::Dispatcher> dispatcher_;
  std::shared_ptr<WindowServices> services_;
  CloseGuardSequencer sequencer_;
  std::uint64_t nextId_{0};
  std::vector<std::pair<std::uint64_t, BoolCallback>> focus_;
  std::vector<std::pair<std::uint64_t, BoolCallback>> visibility_;
  std::vector<std::pair<std::uint64_t, InsetCallback>> inset_;
  bool disposed_{false};
};

}  // namespace rnqnx::nitro
