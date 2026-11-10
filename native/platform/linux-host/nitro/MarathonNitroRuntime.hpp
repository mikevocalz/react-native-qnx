#pragma once

#include <jsi/jsi.h>
#include <memory>

namespace margelo::nitro { class Dispatcher; }

namespace rnqnx::nitro {

// The synchronous overload does not install a JavaScript dispatcher.
void installMarathonNitroRuntime(facebook::jsi::Runtime& runtime);
// Async packages use the existing owned Qt/JavaScript dispatcher.
void installMarathonNitroRuntime(
    facebook::jsi::Runtime& runtime,
    std::shared_ptr<margelo::nitro::Dispatcher> dispatcher);

} // namespace rnqnx::nitro
