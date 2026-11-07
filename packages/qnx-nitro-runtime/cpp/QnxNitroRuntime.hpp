#pragma once

#include <jsi/jsi.h>
#include <memory>

namespace margelo::nitro {
class Dispatcher;
}

namespace rnqnx::nitro {

void installQnxNitroRuntime(facebook::jsi::Runtime& runtime);
void installQnxNitroRuntime(
    facebook::jsi::Runtime& runtime,
    std::shared_ptr<margelo::nitro::Dispatcher> dispatcher);

} // namespace rnqnx::nitro
