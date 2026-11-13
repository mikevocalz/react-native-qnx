#pragma once

#include "../RnsvgItems.h"

#include <QHash>
#include <QSet>

namespace rnqnx::views::rnsvg {

// Places the live views of every <ForeignObject> under `root`: the transform
// from the box's own coordinates to the item's parent comes from the same
// viewBox, ancestor matrices and own matrix the painter uses, so the views
// follow the drawing. A ForeignObject that is not reached (display:none, in
// <defs>) is hidden. GUI thread only, outside paint().
void placeForeignObjects(RnsvgRootItem& root);

// The painted extent of `element` in `root` coordinates; empty if the element
// is not part of the visible tree or paints nothing.
QRectF boundsOfElement(const RnsvgRootItem& root, const RnsvgElementItem* element);

// The painted extent of every element in `wanted`, from one traversal of the
// tree (cost is linear in the tree, not in tree size times |wanted|). Elements
// that are not visible or paint nothing are absent from the result. `visits`,
// if given, receives the number of tree nodes the traversal visited.
QHash<const RnsvgElementItem*, QRectF> boundsOfElements(const RnsvgRootItem& root,
                                                        const QSet<const RnsvgElementItem*>& wanted,
                                                        int* visits = nullptr);

}  // namespace rnqnx::views::rnsvg
