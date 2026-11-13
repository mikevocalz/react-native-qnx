#include "ExpoQnxUuid.hpp"

#include <array>
#include <cstdint>
#include <random>

namespace rnqnx::expo {
namespace {

using Bytes = std::array<std::uint8_t, 16>;

std::string format(const Bytes& bytes) {
  static constexpr char kHex[] = "0123456789abcdef";
  std::string out;
  out.reserve(36);
  for (std::size_t i = 0; i < bytes.size(); ++i) {
    if (i == 4 || i == 6 || i == 8 || i == 10) out.push_back('-');
    out.push_back(kHex[bytes[i] >> 4]);
    out.push_back(kHex[bytes[i] & 0x0f]);
  }
  return out;
}

int hexValue(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

std::optional<Bytes> parse(const std::string& text) {
  if (text.size() != 36) return std::nullopt;
  Bytes bytes{};
  std::size_t out = 0;
  for (std::size_t i = 0; i < text.size();) {
    if (i == 8 || i == 13 || i == 18 || i == 23) {
      if (text[i] != '-') return std::nullopt;
      ++i;
      continue;
    }
    const int high = hexValue(text[i]);
    const int low = hexValue(text[i + 1]);
    if (high < 0 || low < 0) return std::nullopt;
    bytes[out++] = static_cast<std::uint8_t>((high << 4) | low);
    i += 2;
  }
  return bytes;
}

std::uint32_t rotl(std::uint32_t value, unsigned bits) {
  return (value << bits) | (value >> (32U - bits));
}

// SHA-1 (FIPS 180-4). Used only for UUID v5, as RFC 9562 requires.
std::array<std::uint8_t, 20> sha1(const std::string& message) {
  std::uint32_t h[5] = {0x67452301U, 0xEFCDAB89U, 0x98BADCFEU, 0x10325476U, 0xC3D2E1F0U};
  std::string data = message;
  const std::uint64_t bitLength = static_cast<std::uint64_t>(message.size()) * 8U;
  data.push_back(static_cast<char>(0x80));
  while (data.size() % 64 != 56) data.push_back('\0');
  for (int shift = 56; shift >= 0; shift -= 8) {
    data.push_back(static_cast<char>((bitLength >> shift) & 0xffU));
  }
  for (std::size_t chunk = 0; chunk < data.size(); chunk += 64) {
    std::uint32_t w[80];
    for (std::size_t i = 0; i < 16; ++i) {
      const auto* p = reinterpret_cast<const unsigned char*>(data.data() + chunk + i * 4);
      w[i] = (std::uint32_t{p[0]} << 24) | (std::uint32_t{p[1]} << 16) |
          (std::uint32_t{p[2]} << 8) | std::uint32_t{p[3]};
    }
    for (std::size_t i = 16; i < 80; ++i) {
      w[i] = rotl(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    }
    std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
    for (std::size_t i = 0; i < 80; ++i) {
      std::uint32_t f = 0;
      std::uint32_t k = 0;
      if (i < 20) {
        f = (b & c) | (~b & d);
        k = 0x5A827999U;
      } else if (i < 40) {
        f = b ^ c ^ d;
        k = 0x6ED9EBA1U;
      } else if (i < 60) {
        f = (b & c) | (b & d) | (c & d);
        k = 0x8F1BBCDCU;
      } else {
        f = b ^ c ^ d;
        k = 0xCA62C1D6U;
      }
      const std::uint32_t temp = rotl(a, 5) + f + e + k + w[i];
      e = d;
      d = c;
      c = rotl(b, 30);
      b = a;
      a = temp;
    }
    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
    h[4] += e;
  }
  std::array<std::uint8_t, 20> digest{};
  for (std::size_t i = 0; i < 5; ++i) {
    digest[i * 4] = static_cast<std::uint8_t>(h[i] >> 24);
    digest[i * 4 + 1] = static_cast<std::uint8_t>(h[i] >> 16);
    digest[i * 4 + 2] = static_cast<std::uint8_t>(h[i] >> 8);
    digest[i * 4 + 3] = static_cast<std::uint8_t>(h[i]);
  }
  return digest;
}

} // namespace

std::string uuidV4() {
  // Every byte comes from std::random_device (the C++ library's
  // nondeterministic source), not from a PRNG seeded once.
  std::random_device device;
  Bytes bytes{};
  for (std::size_t i = 0; i < bytes.size(); i += 4) {
    const std::uint32_t value = device();
    for (std::size_t j = 0; j < 4; ++j) {
      bytes[i + j] = static_cast<std::uint8_t>(value >> (j * 8));
    }
  }
  bytes[6] = static_cast<std::uint8_t>((bytes[6] & 0x0fU) | 0x40U);
  bytes[8] = static_cast<std::uint8_t>((bytes[8] & 0x3fU) | 0x80U);
  return format(bytes);
}

std::optional<std::string> uuidV5(const std::string& name, const std::string& namespaceUuid) {
  const auto ns = parse(namespaceUuid);
  if (!ns) return std::nullopt;
  std::string input(ns->begin(), ns->end());
  input += name;
  const auto digest = sha1(input);
  Bytes bytes{};
  for (std::size_t i = 0; i < bytes.size(); ++i) bytes[i] = digest[i];
  bytes[6] = static_cast<std::uint8_t>((bytes[6] & 0x0fU) | 0x50U);
  bytes[8] = static_cast<std::uint8_t>((bytes[8] & 0x3fU) | 0x80U);
  return format(bytes);
}

} // namespace rnqnx::expo
