// Limits page (shown under More).
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
const { SCENARIOS } = require('../fixtures.js');
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

function LimitationsPage() {
  return h(
    ScrollView,
    { style: { flex: 1 }, contentContainerStyle: { paddingBottom: 24 } },
    h(Heading, {
      title: 'Evidence and unsupported work',
      detail: 'These are planned checks, not results from this binary or a connected Pi.',
    }),
    ...SCENARIOS.map((scenario) =>
      h(
        View,
        {
          key: scenario.id,
          style: {
            padding: 12,
            backgroundColor: PANEL,
            borderRadius: 10,
            marginBottom: 8,
          },
        },
        h(Label, { color: INK }, scenario.id + ' / ' + scenario.title + ' / ' + scenario.status),
        h(Label, null, scenario.expectation)
      )
    )
  );
}

module.exports = { LimitsScreen: LimitationsPage };
