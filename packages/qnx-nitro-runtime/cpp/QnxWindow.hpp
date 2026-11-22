#pragma once
#include "generated/WindowBindings.hpp"
#include "WindowSession.h"
#include <NitroModules/Dispatcher.hpp>

namespace rnqnx::runtime { class QtReactHost; }
namespace rnqnx::nitro::windowapi {
// Installs after the Nitro core. Binds admission shutdown to host teardown;
// no Qt object is touched by the teardown hook on the JS owner thread.
void installWindowHost(rnqnx::runtime::QtReactHost& host,
                      const std::shared_ptr<rnqnx::window::WindowSessionState>& state);
} // namespace rnqnx::nitro::windowapi
