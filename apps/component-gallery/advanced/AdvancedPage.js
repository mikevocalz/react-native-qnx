// The gallery's Advanced tab: one scrolling column of cards from ./index.js.
const React = require('react');
const { ScrollView, View, Text } = require('react-native');
const { CARDS } = require('./index.js');

const h = React.createElement;

function AdvancedPage() {
  return h(
    ScrollView,
    { style: { flex: 1 }, contentContainerStyle: { paddingBottom: 24 } },
    h(
      Text,
      { style: { color: '#79e8dc', fontSize: 14, marginBottom: 4 } },
      'ADVANCED / animation and list libraries'
    ),
    ...CARDS.map((card) => h(View, { key: card.id }, h(card.component)))
  );
}

module.exports = { AdvancedPage };
