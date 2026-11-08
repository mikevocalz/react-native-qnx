#include "../../vnext/probes/playground/ApplicationIdentity.h"

#include <cstdio>
#include <stdexcept>
#include <string>

int main() {
  using rnqnx::application::isValidAppId;
  using rnqnx::application::isValidComponentName;
  unsigned checks = 0;
  const auto check = [&checks](bool condition) {
    ++checks;
    if (!condition) throw std::runtime_error("Application identity assertion failed");
  };
  for (const auto* value : {"org.rnqnx.example", "com.Example.task_app", "io.test-app.Main"}) {
    check(isValidAppId(value));
  }
  for (const auto* value : {"", "name", ".io.name", "io..name", "io.name.", "io.1name",
           "io.name.desktop", "io/name.app", "io.name;command", "io.name\nother"}) {
    check(!isValidAppId(value));
  }
  check(!isValidAppId(std::string(256, 'a')));
  check(!isValidAppId(std::string("io.name\0hidden", 14)));
  for (const auto* value : {"main", "ConsumerRoot", "_Root", "$Root", "App.View-1"}) {
    check(isValidComponentName(value));
  }
  for (const auto* value : {"", "1Root", "App Root", "../Root", "Root\n", "Root/Child"}) {
    check(!isValidComponentName(value));
  }
  check(!isValidComponentName(std::string(129, 'a')));
  std::printf("Application identity: %u native validation checks passed.\n", checks);
}
