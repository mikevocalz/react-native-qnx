#include <rnqnx/testing/NodeQuery.h>

#include "TreeScan.h"

namespace rnqnx::testing {

QObject* findNode(QObject* root, const NodeQuery& query) {
  detail::checkQuery(root, query, "findNode");
  const detail::ScanOutcome outcome = detail::scanTree(root, query);
  detail::throwIfUnsupported(outcome, "findNode");
  return outcome.node;
}

}  // namespace rnqnx::testing
