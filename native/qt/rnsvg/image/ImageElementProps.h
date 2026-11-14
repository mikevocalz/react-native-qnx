#pragma once

#include "../RnsvgModel.h"

namespace rnqnx::views::rnsvg {

// Called by the bridge after it has set `next.imageUri` from a new commit.
// If the URI is the one `previous` already loaded (or is loading), the image
// and state carry over, so a commit that only moves the element does not
// reload it. A new URI clears the image and goes to Loading, or to Idle when
// the URI is empty; the element's ImageBinding then starts the load.
void carryImageLoadState(ElementProps& next, const ElementProps& previous);

}  // namespace rnqnx::views::rnsvg
