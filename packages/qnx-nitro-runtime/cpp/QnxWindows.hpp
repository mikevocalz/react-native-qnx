#pragma once

// HybridClass for the windows API. Thin: every call forwards to the backend
// of the installed runtime (QnxWindowsBackend). Spec: specs/QnxWindows.nitro.ts.

#include "QnxWindowsBackend.hpp"

#include <NitroModules/HybridClass.hpp>
#include <NitroModules/HybridObject.hpp>

#include <memory>
#include <string>
#include <vector>

namespace rnqnx::nitro {

class QnxWindows final : public margelo::nitro::HybridObject {
 public:
  QnxWindows() = default;

  struct Class {
    using Base = margelo::nitro::HybridObject::Class;
    static constexpr const char* name = "QnxWindows";
    static const std::shared_ptr<const margelo::nitro::HybridClass>& get();
  };

  // Throws `WINDOWS_UNAVAILABLE: ...` when no backend is installed.
  static std::shared_ptr<QnxWindows> create();

  std::vector<std::string> getCapabilities();
  WindowStateData getState();
  margelo::nitro::Promise<void> setControlsLayout(std::string layout);
  margelo::nitro::Promise<void> setTitle(std::string title);
  margelo::nitro::Promise<Rect> setBounds(Rect bounds);
  margelo::nitro::Promise<void> minimize();
  margelo::nitro::Promise<bool> close(bool force);
  double addCloseGuard(margelo::nitro::Callback<void(double, double, std::string, bool, double)> guard);
  void resolveCloseGuard(double requestId, double guardId, bool allow);
  double addOnFocusChangedListener(margelo::nitro::Callback<void(bool)> listener);
  double addOnVisibilityChangedListener(margelo::nitro::Callback<void(bool)> listener);
  TitleBarInsetData getTitleBarInset();
  double addOnInsetChangedListener(margelo::nitro::Callback<void(double, double, double, double, double, double, double, double)> listener);
  void removeListener(double id);

 protected:
  const std::shared_ptr<const margelo::nitro::HybridClass>& getHybridClass() override {
    return Class::get();
  }

 private:
  std::shared_ptr<QnxWindowsBackend> backend() const;
};

}  // namespace rnqnx::nitro
