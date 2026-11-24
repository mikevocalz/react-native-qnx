#pragma once

// Plain data for the close path. No Qt: the Nitro adapter includes it.

#include <cstdint>
#include <vector>

namespace rnqnx::window {

enum class CloseReason { Control, App, System };
inline const char* closeReasonName(CloseReason reason) {
  switch (reason) {
    case CloseReason::Control: return "control";
    case CloseReason::App: return "app";
    case CloseReason::System: return "system";
  }
  return "control";
}

struct CloseRequestInfo {
  std::uint64_t requestId{0};
  std::uint64_t epoch{0};
  CloseReason reason{CloseReason::Control};
  // False when the source ends the process whatever the guards say.
  bool cancellable{true};
  // Milliseconds until the source kills the process; -1 when it states none.
  int deadlineMs{-1};
  // Registered guard ids in registration order, at the time of the request.
  std::vector<std::uint64_t> guardIds;
};

}  // namespace rnqnx::window
