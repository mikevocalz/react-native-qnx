// @ts-nocheck: test harness around untyped Babel, Yoga and react-test-renderer.
// Lays out a react-test-renderer tree with the real Yoga engine (yoga-layout,
// the same flexbox implementation React Native's Fabric renderer compiles in).
// Host elements become Yoga nodes the way Fabric builds shadow nodes from view
// props: `style` maps to node style, `Text` leaves get a text measure function,
// and `onLayout` is called with the computed frame until no frame changes, as
// the native side does after every commit.
import Yoga, {
  Align,
  Direction,
  Display,
  Edge,
  FlexDirection,
  Gutter,
  Justify,
  MeasureMode,
  Overflow,
  PositionType,
  Wrap,
} from 'yoga-layout';

const config = Yoga.Config.create();
config.setPointScaleFactor(1);

const ALIGN = {
  auto: Align.Auto,
  'flex-start': Align.FlexStart,
  center: Align.Center,
  'flex-end': Align.FlexEnd,
  stretch: Align.Stretch,
  baseline: Align.Baseline,
  'space-between': Align.SpaceBetween,
  'space-around': Align.SpaceAround,
};
const JUSTIFY = {
  'flex-start': Justify.FlexStart,
  center: Justify.Center,
  'flex-end': Justify.FlexEnd,
  'space-between': Justify.SpaceBetween,
  'space-around': Justify.SpaceAround,
  'space-evenly': Justify.SpaceEvenly,
};
const DIRECTION = {
  row: FlexDirection.Row,
  'row-reverse': FlexDirection.RowReverse,
  column: FlexDirection.Column,
  'column-reverse': FlexDirection.ColumnReverse,
};

/** Leaf host types with an intrinsic size when the style gives none. */
const INTRINSIC = {
  Switch: { width: 51, height: 31 },
  ActivityIndicator: { width: 36, height: 36 },
  TextInput: { height: 36 },
};

/** @param {any} style */
export function flattenStyle(style) {
  if (typeof style === 'function') return flattenStyle(style({ pressed: false }));
  if (Array.isArray(style)) return Object.assign({}, ...style.map(flattenStyle));
  return style && typeof style === 'object' ? style : {};
}

/** @param {any} node @param {string} setter @param {any} value */
function setSize(node, setter, value) {
  if (value === undefined || value === null) return;
  if (value === 'auto') node[`${setter}Auto`]?.();
  else if (typeof value === 'string' && value.endsWith('%'))
    node[`${setter}Percent`](parseFloat(value));
  else if (typeof value === 'number') node[setter](value);
}

/** @param {any} node @param {Record<string, any>} style */
function applyStyle(node, style) {
  setSize(node, 'setWidth', style.width);
  setSize(node, 'setHeight', style.height);
  setSize(node, 'setMinWidth', style.minWidth);
  setSize(node, 'setMinHeight', style.minHeight);
  setSize(node, 'setMaxWidth', style.maxWidth);
  setSize(node, 'setMaxHeight', style.maxHeight);
  setSize(node, 'setFlexBasis', style.flexBasis);
  if (typeof style.flex === 'number') node.setFlex(style.flex);
  if (typeof style.flexGrow === 'number') node.setFlexGrow(style.flexGrow);
  if (typeof style.flexShrink === 'number') node.setFlexShrink(style.flexShrink);
  if (style.flexDirection) node.setFlexDirection(DIRECTION[style.flexDirection]);
  node.setFlexWrap(
    style.flexWrap === 'wrap'
      ? Wrap.Wrap
      : style.flexWrap === 'wrap-reverse'
        ? Wrap.WrapReverse
        : Wrap.NoWrap
  );
  if (style.justifyContent) node.setJustifyContent(JUSTIFY[style.justifyContent]);
  if (style.alignItems) node.setAlignItems(ALIGN[style.alignItems]);
  if (style.alignSelf) node.setAlignSelf(ALIGN[style.alignSelf]);
  if (style.alignContent) node.setAlignContent(ALIGN[style.alignContent]);
  node.setPositionType(
    style.position === 'absolute' ? PositionType.Absolute : PositionType.Relative
  );
  node.setDisplay(style.display === 'none' ? Display.None : Display.Flex);
  node.setOverflow(
    style.overflow === 'hidden'
      ? Overflow.Hidden
      : style.overflow === 'scroll'
        ? Overflow.Scroll
        : Overflow.Visible
  );
  if (typeof style.aspectRatio === 'number') node.setAspectRatio(style.aspectRatio);
  for (const [name, edge] of [
    ['left', Edge.Left],
    ['right', Edge.Right],
    ['top', Edge.Top],
    ['bottom', Edge.Bottom],
    ['start', Edge.Start],
    ['end', Edge.End],
  ]) {
    const value = style[name];
    if (typeof value === 'number') node.setPosition(edge, value);
    else if (typeof value === 'string' && value.endsWith('%'))
      node.setPositionPercent(edge, parseFloat(value));
  }
  /** @type {Array<[string, number]>} */
  const boxEdges = [
    ['', Edge.All],
    ['Horizontal', Edge.Horizontal],
    ['Vertical', Edge.Vertical],
    ['Left', Edge.Left],
    ['Right', Edge.Right],
    ['Top', Edge.Top],
    ['Bottom', Edge.Bottom],
    ['Start', Edge.Start],
    ['End', Edge.End],
  ];
  for (const [suffix, edge] of boxEdges) {
    const margin = style[`margin${suffix}`];
    if (typeof margin === 'number') node.setMargin(edge, margin);
    else if (margin === 'auto') node.setMarginAuto(edge);
    else if (typeof margin === 'string' && margin.endsWith('%'))
      node.setMarginPercent(edge, parseFloat(margin));
    const padding = style[`padding${suffix}`];
    if (typeof padding === 'number') node.setPadding(edge, padding);
    else if (typeof padding === 'string' && padding.endsWith('%'))
      node.setPaddingPercent(edge, parseFloat(padding));
    const border = style[suffix === '' ? 'borderWidth' : `border${suffix}Width`];
    if (typeof border === 'number' && suffix !== 'Horizontal' && suffix !== 'Vertical')
      node.setBorder(edge, border);
  }
  if (typeof style.gap === 'number') node.setGap(Gutter.All, style.gap);
  if (typeof style.rowGap === 'number') node.setGap(Gutter.Row, style.rowGap);
  if (typeof style.columnGap === 'number') node.setGap(Gutter.Column, style.columnGap);
}

/** Text a `Text` host shows: its own strings and the strings of nested `Text`. @param {any} instance @returns {string} */
function textOf(instance) {
  return instance.children
    .map((/** @type {any} */ child) => (typeof child === 'string' ? child : textOf(child)))
    .join('');
}

/** Greedy word wrap with a fixed advance of 0.55 em per character. */
function measureText(
  /** @type {string} */ text,
  /** @type {Record<string, any>} */ style,
  /** @type {any} */ props,
  /** @type {number} */ width,
  /** @type {number} */ widthMode
) {
  const size = typeof style.fontSize === 'number' ? style.fontSize : 14;
  const lineHeight =
    typeof style.lineHeight === 'number' ? style.lineHeight : Math.round(size * 1.25);
  const advance = size * 0.55;
  const words = text.split(/\s+/).filter(Boolean);
  const limit = widthMode === MeasureMode.Undefined ? Infinity : width;
  let lines = 1;
  let current = 0;
  let widest = 0;
  for (const word of words) {
    const wordWidth = word.length * advance;
    const next = current === 0 ? wordWidth : current + advance + wordWidth;
    if (next > limit && current > 0) {
      lines += 1;
      current = wordWidth;
    } else {
      current = next;
    }
    widest = Math.max(widest, current);
  }
  if (typeof props.numberOfLines === 'number' && props.numberOfLines > 0)
    lines = Math.min(lines, props.numberOfLines);
  const measuredWidth = widthMode === MeasureMode.Exactly ? width : Math.min(widest, limit);
  return { width: Math.ceil(measuredWidth), height: lines * lineHeight };
}

/**
 * @typedef {{ path: string, type: string, props: any, depth: number, parent: LayoutBox | null, x: number, y: number, width: number, height: number, children: LayoutBox[] }} LayoutBox
 */

/**
 * @param {any} instance a ReactTestInstance
 * @param {string} path
 * @param {LayoutBox | null} parent
 * @param {any[]} yogaNodes freed after the pass
 * @returns {{ yoga: any, box: LayoutBox } | null}
 */
function buildHost(instance, path, parent, yogaNodes) {
  const type = instance.type;
  const props = instance.props;
  const yoga = Yoga.Node.create(config);
  yogaNodes.push(yoga);
  const style = flattenStyle(props.style);
  applyStyle(yoga, style);
  const intrinsic = INTRINSIC[type];
  if (intrinsic) {
    if (style.width === undefined && intrinsic.width !== undefined) yoga.setWidth(intrinsic.width);
    if (style.height === undefined && intrinsic.height !== undefined)
      yoga.setHeight(intrinsic.height);
  }
  // react-native-svg sizes its root from width/height props, not style.
  if (type === 'Svg') {
    setSize(yoga, 'setWidth', props.width);
    setSize(yoga, 'setHeight', props.height);
  }
  /** @type {LayoutBox} */
  const box = {
    path,
    type,
    props,
    depth: parent ? parent.depth + 1 : 0,
    parent,
    x: 0,
    y: 0,
    width: 0,
    height: 0,
    children: [],
  };
  if (type === 'Text') {
    const text = textOf(instance);
    yoga.setMeasureFunc((width, widthMode) => measureText(text, style, props, width, widthMode));
    return { yoga, box };
  }
  // react-native-svg draws its children itself; they are not Yoga nodes.
  if (type !== 'Svg') collectChildren(instance, path, box, yoga, yogaNodes);
  return { yoga, box };
}

/** Walk through composite instances to the host elements below. */
function collectChildren(
  /** @type {any} */ instance,
  /** @type {string} */ path,
  /** @type {LayoutBox} */ box,
  /** @type {any} */ yoga,
  /** @type {any[]} */ yogaNodes
) {
  let index = 0;
  const visit = (/** @type {any} */ node, /** @type {string} */ at) => {
    if (typeof node === 'string') return;
    if (typeof node.type === 'string') {
      const built = buildHost(node, at, box, yogaNodes);
      if (!built) return;
      yoga.insertChild(built.yoga, yoga.getChildCount());
      box.children.push(built.box);
    } else {
      node.children.forEach((/** @type {any} */ child, /** @type {number} */ i) =>
        visit(child, `${at}.${i}`)
      );
    }
  };
  instance.children.forEach((/** @type {any} */ child) => {
    visit(child, `${path}/${index}`);
    index += 1;
  });
}

/** @param {any} yoga @param {LayoutBox} box @param {number} left @param {number} top */
function readLayout(yoga, box, left, top) {
  const layout = yoga.getComputedLayout();
  box.x = left + layout.left;
  box.y = top + layout.top;
  box.width = layout.width;
  box.height = layout.height;
  box.children.forEach((child, i) => readLayout(yoga.getChild(i), child, box.x, box.y));
}

/** Frames sent to `onLayout`, by node path; the last frame sent wins. @type {WeakMap<object, Map<string, string>>} */
const reported = new WeakMap();

/**
 * Lays the renderer's host tree out inside a `width` x `height` window and
 * feeds `onLayout` until the frames settle. Returns the root boxes.
 * @param {any} renderer a react-test-renderer instance
 * @param {{ width: number, height: number }} window
 * @param {(fn: () => void) => void} act
 * @returns {LayoutBox}
 */
export function layoutWindow(renderer, window, act) {
  if (!reported.has(renderer)) reported.set(renderer, new Map());
  const sent = /** @type {Map<string, string>} */ (reported.get(renderer));
  for (let pass = 0; pass < 20; pass += 1) {
    const yogaNodes = /** @type {any[]} */ ([]);
    const root = Yoga.Node.create(config);
    yogaNodes.push(root);
    root.setWidth(window.width);
    root.setHeight(window.height);
    /** @type {LayoutBox} */
    const rootBox = {
      path: 'window',
      type: 'Window',
      props: {},
      depth: -1,
      parent: null,
      x: 0,
      y: 0,
      width: window.width,
      height: window.height,
      children: [],
    };
    collectChildren(renderer.root, 'r', rootBox, root, yogaNodes);
    root.calculateLayout(window.width, window.height, Direction.LTR);
    rootBox.children.forEach((child, i) => readLayout(root.getChild(i), child, 0, 0));
    const events = /** @type {Array<() => void>} */ ([]);
    const visit = (/** @type {LayoutBox} */ box) => {
      if (typeof box.props.onLayout === 'function') {
        const parentX = box.parent ? box.parent.x : 0;
        const parentY = box.parent ? box.parent.y : 0;
        const frame = {
          x: box.x - parentX,
          y: box.y - parentY,
          width: box.width,
          height: box.height,
        };
        const key = JSON.stringify(frame);
        if (sent.get(box.path) !== key) {
          sent.set(box.path, key);
          events.push(() => box.props.onLayout({ nativeEvent: { layout: frame } }));
        }
      }
      box.children.forEach(visit);
    };
    rootBox.children.forEach(visit);
    root.freeRecursive();
    if (events.length === 0) return rootBox;
    act(() => events.forEach((fire) => fire()));
  }
  throw new Error(
    'Layout did not settle in 20 passes: an onLayout handler keeps changing the tree'
  );
}

/** @param {LayoutBox} box @param {(b: LayoutBox) => boolean} match @returns {LayoutBox[]} */
export function findBoxes(box, match) {
  const found = match(box) ? [box] : [];
  for (const child of box.children) found.push(...findBoxes(child, match));
  return found;
}

/**
 * Frames of every box below `box`, relative to `box`, keyed by the box's place
 * in the host tree (child indexes), so equal layouts compare equal whatever
 * composite components sit between the hosts.
 * @param {LayoutBox} box
 * @returns {string[]}
 */
export function frames(box, at = '', origin = { x: box.x, y: box.y }) {
  /** @type {string[]} */
  const lines = [
    `${at || '.'} ${box.type} ${box.x - origin.x} ${box.y - origin.y} ${box.width} ${box.height}`,
  ];
  box.children.forEach((child, i) => lines.push(...frames(child, `${at}/${i}`, origin)));
  return lines;
}
