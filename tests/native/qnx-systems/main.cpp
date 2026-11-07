#include "Probe.h"

#include <exception>
#include <iostream>
#include <string_view>

int main(int argc, char** argv) {
  bool injectTls = false;
  bool injectEof = false;
  if (argc == 3 && std::string_view(argv[1]) == "--inject") {
    injectTls = std::string_view(argv[2]) == "tls-mismatch";
    injectEof = std::string_view(argv[2]) == "early-eof";
  }
  if (argc != 1 && !(argc == 3 && (injectTls || injectEof))) {
    std::cerr << "Usage: rnqnx_systems_probe [--inject tls-mismatch|early-eof]\n";
    return 64;
  }
  try {
    rnqnx::systems::probeThreads(injectTls);
    rnqnx::systems::probeShortIo(injectEof);
    std::cout << "RNQNX_SYSTEMS_OK threads=2 tls-destructors=2 short-read=14 eof=1 cleanup=1\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "RNQNX_SYSTEMS_FAIL " << error.what() << '\n';
    return 1;
  }
}
