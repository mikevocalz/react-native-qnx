#pragma once

namespace rnqnx::views {

/// QML source of the item a React Native Modal mounts as. It draws the modal's
/// background, takes Escape through Qt Quick Keys and emits `requestClose()`.
const char* modalQml();

}  // namespace rnqnx::views
