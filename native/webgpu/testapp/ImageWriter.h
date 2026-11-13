// Writes a readback Image as an uncompressed 32-bit BMP (no dependencies), so
// the receipt can include pixels that came back from the GPU.
#pragma once

#include <string>

#include "Scenarios.h"

namespace rnqnx::webgpu {

bool writeBmp(const std::string& path, const Image& image, std::string& error);

}  // namespace rnqnx::webgpu
