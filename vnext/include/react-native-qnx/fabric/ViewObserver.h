#pragma once

#include <react/renderer/mounting/ShadowView.h>

#include <functional>

namespace rnqnx::fabric {

/// Called by the mount adapter after every created or updated view
/// (`removed == false`) and for every deleted view (`removed == true`, with the
/// old ShadowView). Libraries that need a view's props or event emitter
/// (Gesture Handler's detector) use it; the adapter reads nothing from it.
/// One process-wide observer, called on the GUI thread. Set before mounting
/// starts and clear before the adapter goes away.
using ViewObserver = std::function<void(const facebook::react::ShadowView&, bool removed)>;

void setViewObserver(ViewObserver observer);
const ViewObserver& viewObserver();

}  // namespace rnqnx::fabric
