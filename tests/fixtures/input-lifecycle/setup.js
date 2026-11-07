const fabric = globalThis.nativeFabricUIManager;
fabric.registerEventHandler(() => {});
const children = fabric.createChildSet();
for (const tag of [501, 502]) {
  fabric.appendChildToSet(
    children,
    fabric.createNode(tag, 'View', 11, { width: 160, height: 80, collapsable: false }, {})
  );
}
fabric.completeRoot(11, children);
