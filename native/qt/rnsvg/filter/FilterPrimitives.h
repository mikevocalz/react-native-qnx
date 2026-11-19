#pragma once

// One function per primitive. The engine resolves inputs and the subregion;
// each function returns a region-sized layer whose pixels outside `sub` it
// may leave unspecified (the engine clears them). `cs` is the primitive's
// color-interpolation-filters value.
//
// A function that finds its parameters invalid returns the input image
// (blur, morphology, color matrix and similar, as Filter Effects Level 1
// says) or a transparent image (turbulence, convolve matrix, lighting and
// the other primitives that have no input to fall back on).

#include "FilterInternal.h"

#include <vector>

namespace rnqnx::views::rnsvg::filter::detail {

// The color of a flood-color or lighting-color value; `fallback` for a brush
// that is not a color.
QColor brushColor(const Brush& brush, const Env& env, const QColor& fallback);

Layer applyBlend(const Env&, const Blend&, ColorSpace cs, const QRect& sub, const Layer& in, const Layer& in2);
Layer applyColorMatrix(const Env&, const ColorMatrix&, ColorSpace cs, const Layer& in);
Layer applyComponentTransfer(const Env&, const ComponentTransfer&, ColorSpace cs, const Layer& in);
Layer applyComposite(const Env&, const Composite&, ColorSpace cs, const Layer& in, const Layer& in2);
Layer applyConvolveMatrix(const Env&, const ConvolveMatrix&, ColorSpace cs, const QRect& sub, const Layer& in);
Layer applyDisplacementMap(const Env&, const DisplacementMap&, ColorSpace cs, const QRect& sub, const Layer& in,
                           const Layer& in2);
Layer applyDropShadow(const Env&, const DropShadow&, ColorSpace cs, const Layer& in);
Layer applyFlood(const Env&, const Flood&, const QRect& sub);
Layer applyGaussianBlur(const Env&, const GaussianBlur&, ColorSpace cs, const Layer& in);
Layer applyImage(const Env&, const Image&, const QRect& sub, const QRectF& userSubregion);
Layer applyLighting(const Env&, const DiffuseLighting&, ColorSpace cs, const QRect& sub, const Layer& in);
Layer applyLighting(const Env&, const SpecularLighting&, ColorSpace cs, const QRect& sub, const Layer& in);
Layer applyMerge(const Env&, ColorSpace cs, const std::vector<const Layer*>& nodes);
Layer applyMorphology(const Env&, const Morphology&, ColorSpace cs, const Layer& in);
Layer applyOffset(const Env&, const Offset&, const Layer& in);
Layer applyTile(const Env&, const QRect& sub, const Layer& in);
Layer applyTurbulence(const Env&, const Turbulence&, ColorSpace cs, const QRect& sub, const QRectF& userSubregion);

}  // namespace rnqnx::views::rnsvg::filter::detail
