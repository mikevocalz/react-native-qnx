// Shared pieces of the gallery screens: palette, card grid maths, the one
// zustand store and the small text and button helpers.
const React = require('react');
const { Text, View, Pressable } = require('react-native');
const { create } = require('zustand');

const h = React.createElement;
const INK = '#e9f0f7';
const MUTED = '#9dafc2';
const ACCENT = '#79e8dc';
const PANEL = '#142538';
const BACKGROUND = '#0c1929';
const SELECTED = '#1b5b61';
const GAP = 20;

// Columns for the card grid. Card widths are whole pixels computed from the
// measured content width: percentage widths that add up to exactly 100%
// (48% + 2% margin) can overflow by a rounding error in Yoga and wrap every
// card onto its own line, which left the right half of a 1920 px window empty.
function gridColumns(width) {
  if (width >= 1500) return 3;
  if (width >= 900) return 2;
  return 1;
}

const useConformance = create((set) => ({
  page: 'gallery',
  modal: false,
  row: null,
  text: 'QNX input sample',
  toggle: false,
  presses: 0,
  goTo: (page) => set({ page }),
  chooseRow: (row) => set({ row }),
  setText: (text) => set({ text }),
  setToggle: (toggle) => set({ toggle }),
  setModal: (modal) => set({ modal }),
  press: () => set((state) => ({ presses: state.presses + 1 })),
}));

function Label({ children, color = MUTED, size = 14, style }) {
  return h(Text, { style: [{ color, fontSize: size, lineHeight: size + 6 }, style] }, children);
}

function Heading({ title, detail }) {
  return h(
    View,
    { style: { marginBottom: 12 } },
    h(Label, { color: INK, size: 21 }, title),
    h(Label, null, detail)
  );
}

function ActionButton({ title, onPress }) {
  return h(
    Pressable,
    {
      onPress,
      accessible: true,
      accessibilityRole: 'button',
      accessibilityLabel: title,
      style: ({ pressed }) => ({
        alignSelf: 'flex-start',
        minHeight: 44,
        paddingHorizontal: 20,
        justifyContent: 'center',
        backgroundColor: pressed ? '#237a80' : SELECTED,
        borderRadius: 10,
        marginBottom: 12,
      }),
    },
    h(Label, { color: '#f2ffff', size: 15 }, title)
  );
}

// The size of the Screen a card grid sits in. Every mounted page (the five
// tabs and any drawer destination already visited) measures itself, so each
// grid reads its own page's width. A single shared store let the last page to
// lay out set the width for all of them, and a full-width drawer page made the
// tab pages draw cards as wide as the window instead of window minus the rail.
const ViewportContext = React.createContext({ width: 0, height: 0 });

function useViewport() {
  return React.useContext(ViewportContext);
}

// A tab or drawer page: the gallery background with the page padding, and the
// page's own measured size for the grids below it.
function Screen({ children }) {
  const [size, setSize] = React.useState({ width: 0, height: 0 });
  const onLayout = React.useCallback((event) => {
    const width = Math.round(event.nativeEvent.layout.width);
    const height = Math.round(event.nativeEvent.layout.height);
    if (!(width > 0 && height > 0)) return;
    setSize((previous) =>
      previous.width === width && previous.height === height ? previous : { width, height }
    );
  }, []);
  return h(
    View,
    {
      collapsable: false,
      style: { flex: 1, backgroundColor: BACKGROUND, paddingHorizontal: 28, paddingTop: 18 },
      onLayout,
    },
    h(ViewportContext.Provider, { value: size }, children)
  );
}

module.exports = {
  h,
  Screen,
  useViewport,
  INK,
  MUTED,
  ACCENT,
  PANEL,
  BACKGROUND,
  SELECTED,
  GAP,
  gridColumns,
  useConformance,
  Label,
  Heading,
  ActionButton,
};
