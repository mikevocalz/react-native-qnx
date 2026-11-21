#pragma once

#include <rnqnx/testing/NodeQuery.h>

namespace rnqnx::testing::detail {

struct ScanOutcome {
  QObject* node = nullptr;       // first object satisfying every field
  bool nameMatchedHidden = false;  // a name match failed only requireVisible
  QString unsupportedClass;        // a name match had no visibility (requireVisible)
};

// Pre-order walk: the object, then QObject::children(), then
// QQuickItem::childItems(). Each object is visited once.
ScanOutcome scanTree(QObject* root, const NodeQuery& query);

// Throws std::logic_error naming the class when a scan found no node but saw a
// name match whose type has no visibility. Never called from inside the loop.
void throwIfUnsupported(const ScanOutcome& outcome, const char* call);

// Argument and thread checks shared by findNode and waitForNode.
void checkQuery(QObject* root, const NodeQuery& query, const char* call);

}  // namespace rnqnx::testing::detail
