#pragma once
#include <optional>
#include <string>
namespace rnqnx::expo {
// Process metadata, not an authorization or entitlement identity.
struct ApplicationMetadata {
  std::optional<std::string> applicationId;
  std::optional<std::string> applicationName;
  std::optional<std::string> nativeApplicationVersion;
};
// Reads on the QGuiApplication owner thread, with no devices or permissions.
ApplicationMetadata readApplicationMetadata();
void registerApplicationModule();
} // namespace rnqnx::expo
