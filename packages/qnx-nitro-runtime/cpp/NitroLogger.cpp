#include "NitroLogger.hpp"

#include <cstdio>

namespace margelo::nitro {

void Logger::nativeLog(
    [[maybe_unused]] LogLevel level,
    [[maybe_unused]] const char* tag,
    [[maybe_unused]] const std::string& message) {
#ifdef NITRO_DEBUG
  std::fprintf(stderr, "[Nitro.%s] %s\n", tag, message.c_str());
#endif
}

} // namespace margelo::nitro
