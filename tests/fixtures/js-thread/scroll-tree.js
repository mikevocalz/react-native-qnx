// A scroll view over a tall column of coloured stripes, and a view that fills
// the surface. The colours make a pan and a layout change visible in a grab.
const fabric = globalThis.nativeFabricUIManager;
const surfaceId = 11;
const scroll = fabric.createNode(
  601,
  'ScrollView',
  surfaceId,
  { width: 200, height: 100, collapsable: false, backgroundColor: 0xffffe082 },
  {}
);
const content = fabric.createNode(
  602,
  'View',
  surfaceId,
  { width: 200, height: 600, collapsable: false, backgroundColor: 0xff90caf9 },
  {}
);
const stripeColours = [0xffef5350, 0xff66bb6a, 0xffab47bc, 0xffffa726, 0xff26c6da, 0xff8d6e63];
for (let i = 0; i < stripeColours.length; i++) {
  fabric.appendChild(
    content,
    fabric.createNode(
      610 + i,
      'View',
      surfaceId,
      { width: 200, height: 100, collapsable: false, backgroundColor: stripeColours[i] },
      {}
    )
  );
}
fabric.appendChild(scroll, content);
const filler = fabric.createNode(
  603,
  'View',
  surfaceId,
  {
    width: '100%',
    height: '100%',
    position: 'absolute',
    top: 0,
    left: 0,
    collapsable: false,
    backgroundColor: 0xfff5f5f5,
  },
  {}
);
const set = fabric.createChildSet();
fabric.appendChildToSet(set, filler);
fabric.appendChildToSet(set, scroll);
fabric.completeRoot(surfaceId, set);
