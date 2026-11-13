#include "ImageWriter.h"

#include <cstdint>
#include <cstdio>
#include <vector>

namespace rnqnx::webgpu {

namespace {
void put32(std::vector<uint8_t>& out, uint32_t v) {
  for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>(v >> (8 * i)));
}
void put16(std::vector<uint8_t>& out, uint16_t v) {
  out.push_back(static_cast<uint8_t>(v));
  out.push_back(static_cast<uint8_t>(v >> 8));
}
}  // namespace

bool writeBmp(const std::string& path, const Image& image, std::string& error) {
  const uint32_t dataSize = image.width * image.height * 4;
  std::vector<uint8_t> file;
  file.reserve(54 + dataSize);
  file.push_back('B');
  file.push_back('M');
  put32(file, 54 + dataSize);
  put32(file, 0);
  put32(file, 54);
  put32(file, 40);
  put32(file, image.width);
  put32(file, static_cast<uint32_t>(-static_cast<int32_t>(image.height)));  // top-down
  put16(file, 1);
  put16(file, 32);
  put32(file, 0);
  put32(file, dataSize);
  put32(file, 2835);
  put32(file, 2835);
  put32(file, 0);
  put32(file, 0);
  for (uint32_t y = 0; y < image.height; ++y) {
    for (uint32_t x = 0; x < image.width; ++x) {
      const Rgba8 p = image.at(x, y);
      file.push_back(p.b);
      file.push_back(p.g);
      file.push_back(p.r);
      file.push_back(p.a);
    }
  }
  std::FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) {
    error = "cannot open " + path;
    return false;
  }
  const bool ok = std::fwrite(file.data(), 1, file.size(), f) == file.size();
  std::fclose(f);
  if (!ok) error = "short write to " + path;
  return ok;
}

}  // namespace rnqnx::webgpu
