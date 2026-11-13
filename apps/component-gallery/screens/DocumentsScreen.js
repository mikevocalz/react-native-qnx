// Documents tab: 1,000 synthetic rows in a FlatList.
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

function ListPage() {
  const selected = useConformance((state) => state.row);
  const choose = useConformance((state) => state.chooseRow);
  return h(
    View,
    { style: { flex: 1, minHeight: 180 } },
    h(Heading, {
      title: 'LIST-001 / 1,000 synthetic documents',
      detail: 'Scroll, select row 0500 and revisit. This does not measure frame presentation.',
    }),
    h(
      Label,
      { color: ACCENT, style: { marginBottom: 10 } },
      selected ? 'Selected: ' + selected : 'Selected: none'
    ),
    h(FlatList, {
      style: { flex: 1, backgroundColor: PANEL, borderRadius: 12 },
      data: ROWS,
      keyExtractor: (item) => item.id,
      initialNumToRender: 12,
      maxToRenderPerBatch: 12,
      windowSize: 5,
      getItemLayout: (_data, index) => ({ length: 58, offset: 58 * index, index }),
      renderItem: ({ item }) =>
        h(
          Pressable,
          {
            onPress: () => choose(item.id),
            accessible: true,
            accessibilityRole: 'button',
            accessibilityLabel: item.title,
            style: {
              height: 58,
              paddingHorizontal: 14,
              justifyContent: 'center',
              backgroundColor: selected === item.id ? '#215861' : PANEL,
            },
          },
          h(Label, { color: INK, size: 15 }, item.title),
          h(Label, { size: 12 }, item.subtitle)
        ),
    })
  );
}

module.exports = { DocumentsScreen: ListPage };
