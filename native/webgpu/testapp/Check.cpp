#include "Check.h"

#include "Log.h"

namespace rnqnx::webgpu {

void Report::begin(const std::string& name) {
  if (open_) end();
  stages_.push_back({name, true, {}});
  open_ = true;
  logLine("STAGE", "begin name=%s", name.c_str());
}

void Report::fail(const std::string& detail) {
  if (!open_) return;
  auto& stage = stages_.back();
  if (stage.passed) stage.detail = detail;
  stage.passed = false;
  logLine("CHECK", "FAIL %s", detail.c_str());
}

void Report::note(const std::string& detail) {
  if (!open_) return;
  auto& stage = stages_.back();
  if (stage.passed && stage.detail.empty()) stage.detail = detail;
  logLine("CHECK", "ok %s", detail.c_str());
}

void Report::end() {
  if (!open_) return;
  const auto& stage = stages_.back();
  logLine("STAGE", "end name=%s result=%s detail=\"%s\"", stage.name.c_str(),
          stage.passed ? "PASS" : "FAIL", stage.detail.c_str());
  open_ = false;
}

bool Report::allPassed() const {
  for (const auto& stage : stages_) {
    if (!stage.passed) return false;
  }
  return !stages_.empty();
}

}  // namespace rnqnx::webgpu
