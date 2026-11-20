#pragma once

#include <jsi/jsi.h>

#include <memory>

namespace margelo::nitro {
class Dispatcher;
}

namespace rnqnx::nitro {

// Registers the MarathonPlatformInfo hybrid class and installs the Nitro v1
// proxy into the runtime, with no dispatcher. Hybrid objects, properties and
// synchronous methods work. Promises and callbacks need the overload below.
void installMarathonNitroRuntime(facebook::jsi::Runtime& runtime);

// Same as above, then installs the dispatcher that Nitro uses to reach the JS
// thread for Promises and callbacks. The caller owns the dispatcher's
// cancellation and teardown: the Marathon host passes a QtNitroDispatcher over
// the QtReactHost task queue, which drains accepted work before the runtime
// goes away. Throws std::invalid_argument for a null dispatcher.
void installMarathonNitroRuntime(
    facebook::jsi::Runtime& runtime,
    std::shared_ptr<margelo::nitro::Dispatcher> dispatcher);

} // namespace rnqnx::nitro
