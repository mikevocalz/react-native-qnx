const fabric = globalThis.nativeFabricUIManager;
const surfaceId = 11;
const scroll = fabric.createNode(601, 'ScrollView', surfaceId,
  { width: 200, height: 100, collapsable: false }, {});
const content = fabric.createNode(602, 'View', surfaceId,
  { width: 200, height: 600, collapsable: false }, {});
fabric.appendChild(scroll, content);
const filler = fabric.createNode(603, 'View', surfaceId,
  { width: '100%', height: '100%', collapsable: false }, {});
const set = fabric.createChildSet();
fabric.appendChildToSet(set, scroll);
fabric.appendChildToSet(set, filler);
fabric.completeRoot(surfaceId, set);
