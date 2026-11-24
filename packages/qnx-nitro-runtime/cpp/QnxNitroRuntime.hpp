#pragma once

#include <jsi/jsi.h>
#include <memory>

namespace margelo::nitro {
class Dispatcher;
}

namespace rnqnx::nitro {

class QnxWindowsBackend;
struct WindowServices;

void installQnxNitroRuntime(facebook::jsi::Runtime& runtime);
void installQnxNitroRuntime(
    facebook::jsi::Runtime& runtime,
    std::shared_ptr<margelo::nitro::Dispatcher> dispatcher);

// Registers the QnxWindows HybridClass and connects it to the host's window
// services. Call after the dispatcher overload above, on the JavaScript
// thread. Returns the backend; the host calls teardown() on it from its
// QtReactHost teardown hook, before Hermes is destroyed. A host built without
// window chrome never calls this, and `new QnxWindows()` then rejects with
// WINDOWS_UNAVAILABLE.
std::shared_ptr<QnxWindowsBackend> installQnxWindows(
    std::shared_ptr<margelo::nitro::Dispatcher> dispatcher,
    std::shared_ptr<WindowServices> services);

} // namespace rnqnx::nitro
