// Advanced tab: every card in components/advanced/*.card.js.
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
const { ADVANCED_CARDS } = require('../components/advanced');
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

// Cards registered under components/advanced/ (one *.card.js file each). A
// FlatList rather than a ScrollView, so a card may itself hold a vertical
// VirtualizedList (React Native forbids those inside a plain ScrollView).
function AdvancedPage() {
  const width = useConformance((state) => state.width);
  const inner = Math.max(0, width - 2 * 28);
  const columns = gridColumns(inner);
  const cardWidth = Math.floor((inner - GAP * (columns - 1)) / columns);
  const count = ADVANCED_CARDS.length;
  return h(FlatList, {
    key: 'advanced-' + columns,
    style: { flex: 1 },
    contentContainerStyle: { paddingBottom: 24 },
    data: ADVANCED_CARDS,
    numColumns: columns,
    keyExtractor: (card) => card.id,
    ListHeaderComponent: h(Heading, {
      title: 'Advanced',
      detail: count + (count === 1 ? ' card' : ' cards') + ' from components/advanced/*.card.js',
    }),
    renderItem: ({ item: card, index }) =>
      h(
        View,
        {
          // A card with `framed: false` draws its own frame and title.
          style:
            card.framed === false
              ? {
                  width: inner > 0 ? cardWidth : '100%',
                  marginRight: index % columns === columns - 1 ? 0 : GAP,
                }
              : {
                  width: inner > 0 ? cardWidth : '100%',
                  padding: 22,
                  marginBottom: GAP,
                  marginRight: index % columns === columns - 1 ? 0 : GAP,
                  minHeight: 236,
                  borderRadius: 14,
                  backgroundColor: PANEL,
                },
        },
        card.framed !== false && h(Heading, { title: card.title, detail: card.detail }),
        h(card.Component)
      ),
  });
}

module.exports = { AdvancedScreen: AdvancedPage };
