// SectionList page (shown under More).
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
const { createDocumentRows } = require('../fixtures.js');
const ROWS = createDocumentRows();
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
} = ui;

function SectionsPage() {
  const selected = useConformance((state) => state.row);
  const choose = useConformance((state) => state.chooseRow);
  return h(
    View,
    { style: { flex: 1 } },
    h(Heading, {
      title: 'SectionList',
      detail: 'Native scrolling with grouped React Native rows. Select a row, then change tabs.',
    }),
    h(
      Label,
      { color: ACCENT, style: { marginBottom: 12 } },
      selected ? 'Selected: ' + selected : 'Selected: none'
    ),
    h(SectionList, {
      style: { flex: 1 },
      sections: [
        { title: 'Layout and content', data: ROWS.slice(0, 20) },
        { title: 'Interactive controls', data: ROWS.slice(20, 40) },
      ],
      keyExtractor: (item) => item.id,
      renderSectionHeader: ({ section }) =>
        h(
          View,
          { style: { padding: 14, backgroundColor: '#1b5b61' } },
          h(Label, { color: INK, size: 18 }, section.title)
        ),
      renderItem: ({ item }) =>
        h(
          Pressable,
          {
            onPress: () => choose(item.id),
            accessibilityRole: 'button',
            accessibilityLabel: item.title,
            style: {
              height: 58,
              paddingHorizontal: 16,
              justifyContent: 'center',
              backgroundColor: selected === item.id ? '#215861' : PANEL,
            },
          },
          h(Label, { color: INK }, item.title)
        ),
    })
  );
}

module.exports = { SectionsScreen: SectionsPage };
