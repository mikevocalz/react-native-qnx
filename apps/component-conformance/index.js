// Non-premium React Native components rendered by the existing Fabric/Qt backend.
const React = require('react');
const {
  AppRegistry,
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
const { create } = require('zustand');
const { SCENARIOS, createDocumentRows } = require('./fixtures.js');

const h = React.createElement;
const ROWS = createDocumentRows();
const INK = '#e9f0f7';
const MUTED = '#9dafc2';
const ACCENT = '#79e8dc';
const PANEL = '#142538';

const useConformance = create((set) => ({
  page: 'gallery',
  modal: false,
  row: null,
  text: 'QNX input sample',
  toggle: false,
  presses: 0,
  width: 0,
  goTo: (page) => set({ page }),
  chooseRow: (row) => set({ row }),
  setText: (text) => set({ text }),
  setToggle: (toggle) => set({ toggle }),
  setModal: (modal) => set({ modal }),
  press: () => set((state) => ({ presses: state.presses + 1 })),
  setWidth: (width) => set((state) => (state.width === width ? state : { width })),
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

function GalleryPage() {
  const text = useConformance((state) => state.text);
  const setText = useConformance((state) => state.setText);
  const toggle = useConformance((state) => state.toggle);
  const setToggle = useConformance((state) => state.setToggle);
  const presses = useConformance((state) => state.presses);
  const press = useConformance((state) => state.press);
  const modal = useConformance((state) => state.modal);
  const setModal = useConformance((state) => state.setModal);
  const width = useConformance((state) => state.width);
  const [imageStatus, setImageStatus] = React.useState('Waiting for image decode');
  const card = (key, title, detail, ...children) =>
    h(
      View,
      {
        key,
        style: {
          width: width >= 900 ? '48%' : '100%',
          padding: 20,
          marginBottom: 16,
          marginRight: width >= 900 ? '2%' : 0,
          minHeight: 220,
          borderRadius: 14,
          backgroundColor: PANEL,
        },
      },
      h(Heading, { title, detail }),
      ...children
    );
  return h(
    ScrollView,
    { style: { flex: 1 }, contentContainerStyle: { paddingBottom: 24 } },
    h(Heading, {
      title: 'React Native components, native Qt Quick pixels',
      detail:
        'Real React state and public component APIs. Select Documents or Sections for virtualized lists.',
    }),
    h(
      View,
      { style: { flexDirection: 'row', flexWrap: 'wrap' } },
      card(
        'layout',
        'View + Text',
        'Yoga layout → Qt Quick items and native text',
        h(Label, { color: INK, size: 26 }, 'Hello, Qt Quick.'),
        h(
          View,
          { style: { flexDirection: 'row', marginTop: 18, height: 48 } },
          ...['#79e8dc', '#69aefa', '#9d87f1'].map((color, index) =>
            h(View, {
              key: color,
              style: { flex: index + 1, marginRight: 8, borderRadius: 8, backgroundColor: color },
            })
          )
        )
      ),
      card(
        'image',
        'Image',
        'Bundled PNG; no remote image request',
        h(
          View,
          { style: { flexDirection: 'row', alignItems: 'center' } },
          h(Image, {
            source: require('./assets/icon.png'),
            resizeMode: 'contain',
            accessibilityLabel: 'React Native logo',
            style: { width: 112, height: 112, marginRight: 18 },
            onLoad: () => setImageStatus('PNG decoded by the image backend'),
            onError: () => setImageStatus('Image unavailable — check deployed assets'),
          }),
          h(Label, { color: ACCENT, style: { flex: 1 } }, imageStatus)
        )
      ),
      card(
        'press',
        'Pressable',
        'Mouse/touch → React handler → native text update',
        h(NavButton, {
          id: 'press',
          title: 'Press count: ' + presses,
          selected: true,
          onChoose: press,
        }),
        h(
          Label,
          null,
          'State is retained when you change tabs. A rendered button alone is not input proof.'
        )
      ),
      card(
        'text',
        'TextInput',
        'Controlled editing through the Qt text backend',
        h(TextInput, {
          value: text,
          onChangeText: setText,
          accessibilityLabel: 'Gallery text input',
          style: {
            height: 52,
            paddingHorizontal: 14,
            fontSize: 18,
            color: INK,
            backgroundColor: '#0c1929',
            borderRadius: 8,
            marginBottom: 12,
          },
        }),
        h(Label, { color: ACCENT }, 'React value: ' + text)
      ),
      card(
        'switch',
        'Switch + ActivityIndicator',
        'Qt-backed native control state and activity item',
        h(
          View,
          { style: { flexDirection: 'row', alignItems: 'center', marginVertical: 12 } },
          h(Switch, {
            value: toggle,
            onValueChange: setToggle,
            accessibilityLabel: 'Gallery switch',
          }),
          h(
            Label,
            { color: INK, style: { marginHorizontal: 16 } },
            toggle ? 'Switch ON' : 'Switch OFF'
          ),
          h(ActivityIndicator, { size: 'large', color: ACCENT })
        ),
        h(Label, null, 'This activity item is not a frame-rate or Reanimated measurement.')
      ),
      card(
        'modal',
        'Modal',
        'React-controlled overlay; not a separate OS window',
        h(NavButton, {
          id: 'modal',
          title: 'Open modal',
          selected: true,
          onChoose: () => setModal(true),
        }),
        h(Label, null, 'Close the overlay and return to the same component state.')
      )
    ),
    h(
      Modal,
      { visible: modal, transparent: true, onRequestClose: () => setModal(false) },
      h(
        View,
        {
          style: {
            flex: 1,
            alignItems: 'center',
            justifyContent: 'center',
            backgroundColor: 'rgba(0,0,0,0.65)',
          },
        },
        h(
          View,
          {
            style: {
              width: '80%',
              maxWidth: 560,
              padding: 28,
              backgroundColor: PANEL,
              borderRadius: 16,
            },
          },
          h(Heading, {
            title: 'React Native Modal',
            detail: 'This content uses the same Fabric / Qt Quick renderer.',
          }),
          h(Label, { color: INK, style: { marginBottom: 20 } }, 'Press count remains ' + presses),
          h(NavButton, {
            id: 'close',
            title: 'Close modal',
            selected: true,
            onChoose: () => setModal(false),
          })
        )
      )
    )
  );
}

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
        h(Label, { color: INK }, scenario.id + ' / ' + scenario.title + ' / ' + scenario.status),
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
    ['gallery', 'Components'],
    ['list', 'Documents'],
    ['sections', 'Sections'],
    ['controls', 'Controls'],
    ['adaptive', 'Adaptive'],
    ['limits', 'Limits'],
  ];
  const content =
    page === 'gallery'
      ? h(GalleryPage)
      : page === 'sections'
        ? h(SectionsPage)
        : page === 'list'
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
    h(Label, { color: ACCENT, size: 13 }, 'REACT NATIVE QNX / FABRIC + YOGA + QT QUICK'),
    h(Label, { color: INK, size: 28, style: { marginTop: 4 } }, 'Component gallery'),
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
      'Experimental component fixture · Fast Refresh is not enabled · Physical interaction requires observation.'
    )
  );
}

AppRegistry.registerComponent('RNQnxPlayground', () => ConformanceApp);
