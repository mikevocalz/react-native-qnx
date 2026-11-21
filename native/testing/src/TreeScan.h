#pragma once

#include <rnqnx/testing/NodeQuery.h>

namespace rnqnx::testing::detail {

struct ScanOutcome {
  QObject* node = nullptr;       // first object satisfying every field
  bool nameMatchedHidden = false;  // a name match failed only requireVisible
};

// Pre-order walk: the object, then QObject::children(), then
// QQuickItem::childItems(). Each object is visited once.
ScanOutcome scanTree(QObject* root, const NodeQuery& query);

// Argument and thread checks shared by findNode and waitForNode.
void checkQuery(QObject* root, const NodeQuery& query, const char* call);

}  // namespace rnqnx::testing::detail
