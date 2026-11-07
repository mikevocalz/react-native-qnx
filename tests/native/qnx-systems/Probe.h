#pragma once

namespace rnqnx::systems {
void probeThreads(bool injectTlsMismatch);
void probeShortIo(bool injectEarlyEof);
}
