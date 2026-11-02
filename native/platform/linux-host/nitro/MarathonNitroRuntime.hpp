#pragma once

#include <jsi/jsi.h>

namespace rnqnx::nitro {

// Experimental synchronous-only Nitro v1 registration for native MarathonOS.
// The QObject/Qt runtime/Wayland app lifecycle is NOT connected yet.
void installMarathonNitroRuntime(facebook::jsi::Runtime& runtime);

} // namespace rnqnx::nitro
