#include <rnqnx/testing/NodeQuery.h>

#include "TreeScan.h"

namespace rnqnx::testing {

QObject* findNode(QObject* root, const NodeQuery& query) {
  detail::checkQuery(root, query, "findNode");
  return detail::scanTree(root, query).node;
}

}  // namespace rnqnx::testing
