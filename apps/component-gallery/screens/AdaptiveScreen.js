// Adaptive layout page (shown under More).
const React = require('react');
const {
  View,
  Text,
  Image,
  Pressable,
  ScrollView,
  FlatList,
  SectionList,
  TextInput,
  Switch,
  ActivityIndicator,
  Modal,
} = require('react-native');
const ui = require('../ui');
const {
  h,
  INK,
  MUTED,
  ACCENT,
  PANEL,
  BACKGROUND,
  GAP,
  gridColumns,
  useConformance,
  Label,
  Heading,
  ActionButton,
  useViewport,
} = ui;

function AdaptivePage() {
  const { width } = useViewport();
  const row = useConformance((state) => state.row);
  const wide = width >= 760;
  const panelStyle = {
    backgroundColor: PANEL,
    padding: 18,
    borderRadius: 12,
    flex: 1,
    minHeight: 132,
  };
  return h(
    ScrollView,
    { style: { flex: 1 }, contentContainerStyle: { flexGrow: 1, paddingBottom: 24 } },
    h(Heading, {
      title: 'LAYOUT-001 / live window layout',
      detail: 'Resize the host. A two-region simulation is not a hinge or multi-display proof.',
    }),
    h(Label, { color: ACCENT, style: { marginBottom: 14 } }, 'Observed width: ' + width),
    h(
      View,
      { style: { flexDirection: wide ? 'row' : 'column', flex: 1 } },
      h(
        View,
        { style: [panelStyle, { marginRight: wide ? 12 : 0, marginBottom: wide ? 0 : 12 }] },
        h(Label, { color: INK, size: 17 }, 'Document'),
        h(Label, null, row ? 'Selected ' + row : 'Select a row on the List page')
      ),
      h(
        View,
        { style: panelStyle },
        h(Label, { color: INK, size: 17 }, 'Inspector'),
        h(Label, null, wide ? 'Wide two-column mode' : 'Narrow stacked mode')
      )
    )
  );
}

module.exports = { AdaptiveScreen: AdaptivePage };
