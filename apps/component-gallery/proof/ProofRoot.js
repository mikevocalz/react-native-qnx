// One Advanced card as the whole app, for per-component screenshots.
// index.js uses it when the bundle was post-processed by
// scripts/vm-proof/build-bundles.sh, which replaces the PROOF_CARD marker with
// a card id. A bundle that still has the marker starts the normal gallery.
const React = require('react');
const { View, Text } = require('react-native');
const { ADVANCED_CARDS } = require('../components/advanced');
const { INK, MUTED, PANEL, BACKGROUND } = require('../ui');

function makeProofRoot(cardId) {
  const card = ADVANCED_CARDS.find((candidate) => candidate.id === cardId);
  function ProofRoot() {
    if (!card) {
      return React.createElement(
        View,
        { style: { flex: 1, backgroundColor: BACKGROUND, padding: 28 } },
        React.createElement(
          Text,
          { style: { color: INK, fontSize: 18 } },
          `No card with id "${cardId}"`
        )
      );
    }
    return React.createElement(
      View,
      { style: { flex: 1, backgroundColor: BACKGROUND, padding: 28 } },
      React.createElement(
        Text,
        { style: { color: MUTED, fontSize: 13, marginBottom: 14 } },
        `Gallery card: ${card.id}`
      ),
      React.createElement(
        View,
        { style: { width: 560, padding: 22, borderRadius: 14, backgroundColor: PANEL } },
        React.createElement(
          Text,
          { style: { color: INK, fontSize: 20, fontWeight: '600' } },
          card.title
        ),
        React.createElement(
          Text,
          { style: { color: MUTED, fontSize: 13, marginBottom: 14 } },
          card.detail
        ),
        React.createElement(card.Component)
      )
    );
  }
  return ProofRoot;
}

module.exports = { makeProofRoot };
