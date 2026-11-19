#include <worklets/Tools/PlatformLogger.h>

#include <cstdio>
#include <string>

namespace worklets {

void PlatformLogger::log(const char* str) {
  std::fprintf(stderr, "[Worklets] %s\n", str);
}

void PlatformLogger::log(const std::string& str) {
  log(str.c_str());
}

void PlatformLogger::log(const double d) {
  std::fprintf(stderr, "[Worklets] %f\n", d);
}

void PlatformLogger::log(const int i) {
  std::fprintf(stderr, "[Worklets] %d\n", i);
}

void PlatformLogger::log(const bool b) {
  log(b ? "true" : "false");
}

}  // namespace worklets
