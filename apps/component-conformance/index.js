// One non-premium Fabric/Qt Quick conformance fixture. All imported React
// Native modules are direct paths because the generic RN entry is not yet
// supported by QNX. No custom renderer, native binding, or device claim.
const React = require('react');
const AppRegistry = require('react-native/Libraries/ReactNative/AppRegistry').AppRegistry;
const View = require('react-native/Libraries/Components/View/View').default;
const Text = require('react-native/Libraries/Text/Text').default;
const Pressable = require('react-native/Libraries/Components/Pressable/Pressable').default;
const ScrollView = require('react-native/Libraries/Components/ScrollView/ScrollView').default;
const FlatList = require('react-native/Libraries/Lists/FlatList').default;
const TextInput = require('react-native/Libraries/Components/TextInput/TextInput').default;
const Switch = require('react-native/Libraries/Components/Switch/Switch').default;
const ActivityIndicator =
  require('react-native/Libraries/Components/ActivityIndicator/ActivityIndicator').default;
const { create } = require('zustand');
const { SCENARIOS, createDocumentRows } = require('./fixtures.js');

const h = React.createElement;
const ROWS = createDocumentRows();
const INK = '#e9f0f7';
const MUTED = '#9dafc2';
const ACCENT = '#79e8dc';
const PANEL = '#142538';

const useConformance = create((set) => ({
  page: 'list',
  row: null,
  text: 'QNX input sample',
  toggle: false,
  presses: 0,
  width: 0,
  goTo: (page) => set({ page }),
  chooseRow: (row) => set({ row }),
  setText: (text) => set({ text }),
  setToggle: (toggle) => set({ toggle }),
  press: () => set((state) => ({ presses: state.presses + 1 })),
  setWidth: (width) =>
    set((state) => (state.width === width ? state : { width })),
}));

function Label({ children, color = MUTED, size = 14, style }) {
  return h(
    Text,
    { style: [{ color, fontSize: size, lineHeight: size + 6 }, style] },
    children
  );
}

function Heading({ title, detail }) {
  return h(
    View,
    { style: { marginBottom: 12 } },
    h(Label, { color: INK, size: 21 }, title),
    h(Label, null, detail)
  );
}

function NavButton({ id, title, selected, onChoose }) {
  return h(
    Pressable,
    {
      onPress: () => onChoose(id),
      accessible: true,
      accessibilityRole: 'button',
      accessibilityLabel: title,
      style: {
        minHeight: 42,
        paddingHorizontal: 16,
        paddingVertical: 10,
        backgroundColor: selected ? '#1b5b61' : PANEL,
        borderRadius: 10,
        marginRight: 8,
        marginBottom: 8,
      },
    },
    h(Label, { color: selected ? '#f2ffff' : INK, size: 14 }, title)
  );
}

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

function ControlsPage() {
  const text = useConformance((state) => state.text);
  const toggle = useConformance((state) => state.toggle);
  const presses = useConformance((state) => state.presses);
  const setText = useConformance((state) => state.setText);
  const setToggle = useConformance((state) => state.setToggle);
  const press = useConformance((state) => state.press);
  return h(
    ScrollView,
    { style: { flex: 1 }, contentContainerStyle: { paddingBottom: 24 } },
    h(Heading, {
      title: 'INPUT-001 / controlled inputs',
      detail: 'Verify text, selection, focus, press and switch state on each named target.',
    }),
    h(Label, { color: INK }, 'Editable text'),
    h(TextInput, {
      value: text,
      onChangeText: setText,
      accessibilityLabel: 'Editable text',
      style: {
        minHeight: 52,
        marginTop: 8,
        marginBottom: 14,
        paddingHorizontal: 14,
        color: INK,
        backgroundColor: PANEL,
        fontSize: 17,
        borderRadius: 10,
      },
    }),
    h(Label, { color: ACCENT }, 'Observed text: ' + text),
    h(
      View,
      {
        style: {
          flexDirection: 'row',
          alignItems: 'center',
          marginTop: 16,
          marginBottom: 16,
        },
      },
      h(Switch, { value: toggle, onValueChange: setToggle }),
      h(Label, { color: INK, style: { marginLeft: 12 } }, toggle ? 'Switch ON' : 'Switch OFF')
    ),
    h(
      Pressable,
      {
        onPress: press,
        accessible: true,
        accessibilityRole: 'button',
        accessibilityLabel: 'Increase press count',
        style: {
          backgroundColor: '#1b5b61',
          paddingVertical: 14,
          paddingHorizontal: 18,
          borderRadius: 10,
          alignSelf: 'flex-start',
        },
      },
      h(Label, { color: INK }, 'Press count: ' + presses)
    ),
    h(
      View,
      { style: { marginTop: 18, alignItems: 'flex-start' } },
      h(ActivityIndicator, { size: 'small', color: ACCENT })
    ),
    h(
      Label,
      { style: { marginTop: 16 } },
      'A passing host interaction does not qualify physical HID, IME or assistive technology.'
    )
  );
}

function AdaptivePage() {
  const width = useConformance((state) => state.width);
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
    View,
    { style: { flex: 1 } },
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
        h(
          Label,
          { color: INK },
          scenario.id + ' / ' + scenario.title + ' / ' + scenario.status
        ),
        h(Label, null, scenario.expectation)
      )
    )
  );
}

function ConformanceApp() {
  const page = useConformance((state) => state.page);
  const goTo = useConformance((state) => state.goTo);
  const setWidth = useConformance((state) => state.setWidth);
  const pages = [
    ['list', 'Documents'],
    ['controls', 'Controls'],
    ['adaptive', 'Adaptive'],
    ['limits', 'Limits'],
  ];
  const content =
    page === 'list'
      ? h(ListPage)
      : page === 'controls'
        ? h(ControlsPage)
        : page === 'adaptive'
          ? h(AdaptivePage)
          : h(LimitationsPage);
  return h(
    View,
    {
      collapsable: false,
      style: {
        flex: 1,
        width: '100%',
        height: '100%',
        backgroundColor: '#0c1929',
        padding: 18,
      },
      onLayout: (event) => {
        const width = event.nativeEvent.layout.width;
        if (Number.isFinite(width) && width > 0) setWidth(Math.round(width));
      },
    },
    h(Label, { color: ACCENT, size: 13 }, 'RNQNX / CONFORMANCE LAB'),
    h(Label, { color: INK, size: 24, style: { marginTop: 4 } }, 'Device qualification'),
    h(
      View,
      { style: { flexDirection: 'row', flexWrap: 'wrap', marginTop: 14 } },
      ...pages.map(([id, title]) =>
        h(NavButton, { key: id, id, title, selected: page === id, onChoose: goTo })
      )
    ),
    h(View, { style: { flex: 1, minHeight: 0 } }, content),
    h(
      Label,
      { size: 12, style: { marginTop: 10 } },
      'Fixture only / device scenarios not-run or blocked until signed evidence exists.'
    )
  );
}

AppRegistry.registerComponent('RNQnxPlayground', () => ConformanceApp);
