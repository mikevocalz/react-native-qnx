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
// This fixture uses Fabric directly, without React's AppRegistry registration.
// Retire its actual mounted tree when the host requests surface teardown.
globalThis.RN$stopSurface = (surfaceId) => {
  fabric.completeRoot(surfaceId, fabric.createChildSet());
};
