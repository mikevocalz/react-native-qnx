#pragma once

#include <optional>
#include <string>

namespace rnqnx::expo {

/** A random (version 4) UUID in lowercase 8-4-4-4-12 form. */
std::string uuidV4();

/** A name-based SHA-1 (version 5) UUID, RFC 9562 section 5.5. Returns nullopt
 * when `namespaceUuid` is not an 8-4-4-4-12 hexadecimal UUID. */
std::optional<std::string> uuidV5(const std::string& name, const std::string& namespaceUuid);

} // namespace rnqnx::expo
