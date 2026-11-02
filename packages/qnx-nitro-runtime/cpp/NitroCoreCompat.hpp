#pragma once

// Nitro v1 alpha NitroLogger.hpp uses std::unique_ptr before including <memory>.
// Keep the upstream compatibility header in our own adapter, not node_modules.
// The scoped CMake preinclude ensures all v1 translation units compile with
// valid standard-library declarations; remove when the upstream alpha is fixed.
#include <memory>
