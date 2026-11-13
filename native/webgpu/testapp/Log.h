// Line-oriented logging for the Stage 5 WebGPU test app. Every line starts
// with "RNQNX_WEBGPU" so a receipt can grep it out of the Dawn and driver noise.
#pragma once

#include <cstdarg>
#include <cstdio>

namespace rnqnx::webgpu {

inline void logLine(const char* kind, const char* format, ...)
    __attribute__((format(printf, 2, 3)));

inline void logLine(const char* kind, const char* format, ...) {
  std::printf("RNQNX_WEBGPU %s ", kind);
  va_list args;
  va_start(args, format);
  std::vprintf(format, args);
  va_end(args);
  std::printf("\n");
  std::fflush(stdout);
}

}  // namespace rnqnx::webgpu
