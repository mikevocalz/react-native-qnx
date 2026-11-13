// Records stage results. A stage passes only when every check inside it held.
#pragma once

#include <string>
#include <vector>

namespace rnqnx::webgpu {

struct StageResult {
  std::string name;
  bool passed = true;
  std::string detail;
};

class Report {
 public:
  void begin(const std::string& name);
  // Marks the current stage failed (and keeps the first failure's text).
  void fail(const std::string& detail);
  void note(const std::string& detail);
  void end();
  bool allPassed() const;
  const std::vector<StageResult>& stages() const { return stages_; }

 private:
  std::vector<StageResult> stages_;
  bool open_ = false;
};

}  // namespace rnqnx::webgpu
