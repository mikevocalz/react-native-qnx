const fabric = globalThis.nativeFabricUIManager;
const node = fabric.createNode(701, 'View', 11,
  { width: 10, height: 10, collapsable: false }, {});
const set = fabric.createChildSet();
fabric.appendChildToSet(set, node);
fabric.completeRoot(11, set);
