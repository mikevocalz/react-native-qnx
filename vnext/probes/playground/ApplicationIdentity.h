#pragma once

#include <string>
#include <string_view>

namespace rnqnx::application {

// A consumer identity is separate from the native binary's legacy probe name.
// Display-name Unicode validation belongs to Qt; identifiers are portable ASCII.
struct Identity {
  std::string appId;
  std::string componentName;
  std::string displayName;
};

inline bool isAsciiLetter(char value) noexcept {
  return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z');
}

inline bool isIdentifierPart(char value) noexcept {
  return isAsciiLetter(value) || (value >= '0' && value <= '9') ||
      value == '_' || value == '-';
}

inline bool isValidAppId(std::string_view value) noexcept {
  if (value.empty() || value.size() > 255 || value.ends_with(".desktop")) return false;
  bool start = true;
  bool dotted = false;
  for (const char character : value) {
    if (character == '.') {
      if (start) return false;
      start = true;
      dotted = true;
    } else {
      if (start ? !isAsciiLetter(character) : !isIdentifierPart(character)) return false;
      start = false;
    }
  }
  return dotted && !start;
}

inline bool isValidComponentName(std::string_view value) noexcept {
  if (value.empty() || value.size() > 128 ||
      !(isAsciiLetter(value.front()) || value.front() == '_' || value.front() == '$')) {
    return false;
  }
  for (const char character : value) {
    if (!(isIdentifierPart(character) || character == '$' || character == '.')) return false;
  }
  return true;
}

}  // namespace rnqnx::application
