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
  useWindowDimensions,
} = require('react-native');
const { create } = require('zustand');
const {
  AdaptiveShell,
  AdaptiveGrid,
  defaultAdaptiveColors,
  getWindowSizeClass,
} = require('../../packages/react-native-qnx/adaptive');
const { SCENARIOS, createDocumentRows } = require('./fixtures.js');

const h = React.createElement;
const ROWS = createDocumentRows();
const COLORS = defaultAdaptiveColors;
const ACCENT = '#79e8dc';
const PAGES = [
  { id: 'gallery', label: 'Components' },
  { id: 'list', label: 'Documents' },
  { id: 'sections', label: 'Sections' },
  { id: 'controls', label: 'Controls' },
  { id: 'adaptive', label: 'Adaptive' },
  { id: 'limits', label: 'Limits' },
];
const inputStyle = {
  minHeight: 48,
  paddingHorizontal: 12,
  paddingVertical: 10,
  fontSize: 17,
  color: COLORS.text,
  backgroundColor: COLORS.background,
  borderRadius: 5,
  borderWidth: 1,
  borderColor: COLORS.border,
  marginBottom: 12,
};

const useConformance = create((set) => ({
  page: 'gallery',
  visited: { gallery: true },
  modal: false,
  row: null,
  text: 'QNX input sample',
  toggle: false,
  presses: 0,
  width: 0,
  height: 0,
  imageStatus: 'Waiting for image decode',
  goTo: (page) => set((state) => ({ page, visited: { ...state.visited, [page]: true } })),
  chooseRow: (row) => set({ row }),
  setText: (text) => set({ text }),
  setToggle: (toggle) => set({ toggle }),
  setModal: (modal) => set({ modal }),
  setImageStatus: (imageStatus) => set({ imageStatus }),
  press: () => set((state) => ({ presses: state.presses + 1 })),
  setContentSize: (width, height) =>
    set((state) => (state.width === width && state.height === height ? state : { width, height })),
}));

function Label({ children, color = COLORS.muted, size = 16, style, ...props }) {
  return h(
    Text,
    { ...props, style: [{ color, fontSize: size, lineHeight: size + 7 }, style] },
    children
  );
}

function Heading({ title, detail }) {
  return h(
    View,
    { style: { marginBottom: 18 } },
    h(
      Label,
      { color: COLORS.text, size: 21, accessibilityRole: 'header', style: { fontWeight: '600' } },
      title
    ),
    detail ? h(Label, { size: 14, style: { marginTop: 3 } }, detail) : null
  );
}

const Action = React.forwardRef(function Action({ title, onPress, primary = false, testID }, ref) {
  return h(
    Pressable,
    {
      ref,
      testID,
      onPress,
      accessible: true,
      accessibilityRole: 'button',
      accessibilityLabel: title,
      style: ({ pressed }) => ({
        minHeight: 44,
        paddingHorizontal: 16,
        paddingVertical: 10,
        borderRadius: 5,
        borderWidth: 1,
        borderColor: primary ? ACCENT : COLORS.border,
        backgroundColor: primary ? ACCENT : pressed ? COLORS.selected : COLORS.background,
        alignSelf: 'flex-start',
        maxWidth: '100%',
      }),
    },
    h(Label, { color: primary ? '#082622' : COLORS.text, style: { fontWeight: '600' } }, title)
  );
});

function ExampleCard({ title, detail, children }) {
  return h(
    View,
    {
      collapsable: false,
      testID: 'example-card-' + title,
      style: {
        padding: 18,
        borderRadius: 6,
        borderWidth: 1,
        borderColor: COLORS.border,
        backgroundColor: COLORS.surface,
        minWidth: 0,
      },
    },
    h(Heading, { title, detail }),
    children
  );
}

function GalleryPage({ openModal }) {
  const text = useConformance((state) => state.text);
  const setText = useConformance((state) => state.setText);
  const toggle = useConformance((state) => state.toggle);
  const setToggle = useConformance((state) => state.setToggle);
  const presses = useConformance((state) => state.presses);
  const press = useConformance((state) => state.press);
  const imageStatus = useConformance((state) => state.imageStatus);
  const setImageStatus = useConformance((state) => state.setImageStatus);
  const modalButton = React.useRef(null);
  return h(
    ScrollView,
    { testID: 'gallery-scroll', style: { flex: 1 }, contentContainerStyle: { paddingBottom: 8 } },
    h(Heading, { title: 'Components', detail: 'Everyday controls, with room to work.' }),
    h(
      AdaptiveGrid,
      { testID: 'adaptive-grid', minItemWidth: 300, gap: 16, maxColumns: 3 },
      h(
        ExampleCard,
        { key: 'press', title: 'Pressable', detail: 'An action with immediate feedback.' },
        h(Action, {
          testID: 'gallery-press',
          title: 'Press count: ' + presses,
          primary: true,
          onPress: press,
        })
      ),
      h(
        ExampleCard,
        {
          key: 'text',
          title: 'TextInput',
          detail: 'Your text stays in place as the layout changes.',
        },
        h(TextInput, {
          testID: 'gallery-text-input',
          value: text,
          onChangeText: setText,
          accessibilityLabel: 'Gallery text input',
          style: inputStyle,
        }),
        h(Label, { size: 14 }, 'React value: ' + text)
      ),
      h(
        ExampleCard,
        {
          key: 'switch',
          title: 'Switch + ActivityIndicator',
          detail: 'Native controls with visible state.',
        },
        h(
          View,
          {
            style: {
              flexDirection: 'row',
              alignItems: 'center',
              flexWrap: 'wrap',
              marginBottom: 8,
            },
          },
          h(Switch, {
            testID: 'gallery-switch',
            value: toggle,
            onValueChange: setToggle,
            accessibilityLabel: 'Gallery switch',
          }),
          h(
            Label,
            { color: COLORS.text, style: { marginHorizontal: 12 } },
            toggle ? 'Switch ON' : 'Switch OFF'
          ),
          h(ActivityIndicator, { size: 'small', color: ACCENT })
        )
      ),
      h(
        ExampleCard,
        { key: 'modal', title: 'Modal', detail: 'A focused dialog sized for the task.' },
        h(Action, {
          ref: modalButton,
          title: 'Open modal',
          onPress: () => openModal(modalButton.current),
        })
      ),
      h(
        ExampleCard,
        {
          key: 'layout',
          title: 'View + Text',
          detail: 'Flexible native layout and readable text.',
        },
        h(Label, { color: COLORS.text, size: 24 }, 'Hello, Qt Quick.'),
        h(
          View,
          { style: { flexDirection: 'row', marginTop: 16, height: 36 } },
          ...[1, 2, 3].map((part) =>
            h(View, {
              key: part,
              style: {
                flex: part,
                marginRight: part < 3 ? 8 : 0,
                borderRadius: 4,
                backgroundColor: part === 2 ? COLORS.selected : ACCENT,
              },
            })
          )
        )
      ),
      h(
        ExampleCard,
        { key: 'image', title: 'Image', detail: 'A bundled asset with its original proportions.' },
        h(Image, {
          source: require('./assets/icon.png'),
          resizeMode: 'contain',
          accessibilityLabel: 'React Native logo',
          style: { width: 88, height: 88, marginBottom: 10 },
          onLoad: () => setImageStatus('PNG decoded by the image backend'),
          onError: () => setImageStatus('Image unavailable — check deployed assets'),
        }),
        h(Label, { size: 14 }, imageStatus)
      )
    )
  );
}

function ListPage() {
  const selected = useConformance((state) => state.row);
  const choose = useConformance((state) => state.chooseRow);
  const { fontScale } = useWindowDimensions();
  const rowHeight = Math.ceil(64 * Math.max(1, fontScale));
  return h(
    View,
    { style: { flex: 1, minHeight: 0 } },
    h(Heading, {
      title: 'Documents',
      detail: '1,000 synthetic documents. Selection and scroll position stay with this page.',
    }),
    h(
      Label,
      { color: ACCENT, size: 14, style: { marginBottom: 12 } },
      selected ? 'Selected: ' + selected : 'Selected: none'
    ),
    h(FlatList, {
      testID: 'document-list',
      style: { flex: 1, minHeight: 0, backgroundColor: COLORS.surface, borderRadius: 6 },
      data: ROWS,
      keyExtractor: (item) => item.id,
      initialNumToRender: 12,
      maxToRenderPerBatch: 12,
      windowSize: 5,
      getItemLayout: (_data, index) => ({ length: rowHeight, offset: rowHeight * index, index }),
      renderItem: ({ item }) =>
        h(
          Pressable,
          {
            onPress: () => choose(item.id),
            accessible: true,
            accessibilityRole: 'button',
            accessibilityLabel: item.title,
            accessibilityState: { selected: selected === item.id },
            style: {
              height: rowHeight,
              paddingHorizontal: 16,
              justifyContent: 'center',
              backgroundColor: selected === item.id ? COLORS.selected : COLORS.surface,
            },
          },
          h(Label, { color: COLORS.text, numberOfLines: 1 }, item.title),
          h(Label, { size: 13, numberOfLines: 1 }, item.subtitle)
        ),
    })
  );
}

function SectionsPage() {
  const selected = useConformance((state) => state.row);
  const choose = useConformance((state) => state.chooseRow);
  return h(
    View,
    { style: { flex: 1, minHeight: 0 } },
    h(Heading, {
      title: 'Sections',
      detail: 'Grouped native list content, with the same selected document.',
    }),
    h(
      Label,
      { color: ACCENT, size: 14, style: { marginBottom: 12 } },
      selected ? 'Selected: ' + selected : 'Selected: none'
    ),
    h(SectionList, {
      style: { flex: 1, minHeight: 0 },
      sections: [
        { title: 'Layout and content', data: ROWS.slice(0, 20) },
        { title: 'Interactive controls', data: ROWS.slice(20, 40) },
      ],
      keyExtractor: (item) => item.id,
      renderSectionHeader: ({ section }) =>
        h(
          View,
          { style: { padding: 14, backgroundColor: COLORS.selected } },
          h(Label, { color: COLORS.text, size: 18 }, section.title)
        ),
      renderItem: ({ item }) =>
        h(
          Pressable,
          {
            onPress: () => choose(item.id),
            accessibilityRole: 'button',
            accessibilityLabel: item.title,
            accessibilityState: { selected: selected === item.id },
            style: {
              minHeight: 52,
              paddingHorizontal: 16,
              paddingVertical: 14,
              justifyContent: 'center',
              backgroundColor: selected === item.id ? COLORS.selected : COLORS.surface,
            },
          },
          h(Label, { color: COLORS.text }, item.title)
        ),
    })
  );
}

function ControlsPage({ openModal }) {
  const text = useConformance((state) => state.text);
  const toggle = useConformance((state) => state.toggle);
  const presses = useConformance((state) => state.presses);
  const setText = useConformance((state) => state.setText);
  const setToggle = useConformance((state) => state.setToggle);
  const press = useConformance((state) => state.press);
  const modalButton = React.useRef(null);
  return h(
    ScrollView,
    { style: { flex: 1 }, contentContainerStyle: { paddingBottom: 16 } },
    h(
      View,
      { style: { width: '100%', maxWidth: 600 } },
      h(Heading, { title: 'Controls', detail: 'The same state, in a focused form.' }),
      h(Label, { color: COLORS.text, style: { marginBottom: 8 } }, 'Editable text'),
      h(TextInput, {
        value: text,
        onChangeText: setText,
        accessibilityLabel: 'Editable text',
        style: inputStyle,
      }),
      h(Label, { size: 14 }, 'Observed text: ' + text),
      h(
        View,
        {
          style: {
            flexDirection: 'row',
            alignItems: 'center',
            flexWrap: 'wrap',
            marginVertical: 20,
          },
        },
        h(Switch, {
          value: toggle,
          onValueChange: setToggle,
          accessibilityLabel: 'Controls switch',
        }),
        h(
          Label,
          { color: COLORS.text, style: { marginLeft: 12 } },
          toggle ? 'Switch ON' : 'Switch OFF'
        )
      ),
      h(Action, { title: 'Press count: ' + presses, primary: true, onPress: press }),
      h(
        View,
        { style: { marginTop: 20 } },
        h(Action, {
          ref: modalButton,
          title: 'Open modal',
          onPress: () => openModal(modalButton.current),
        })
      )
    )
  );
}

function AdaptivePage() {
  const window = useWindowDimensions();
  const row = useConformance((state) => state.row);
  const width = useConformance((state) => state.width);
  const classes = getWindowSizeClass(window);
  return h(
    ScrollView,
    { style: { flex: 1 }, contentContainerStyle: { paddingBottom: 16 } },
    h(Heading, { title: 'Adaptive', detail: 'The same shell and grid used by this gallery.' }),
    h(
      Label,
      { color: ACCENT, size: 14, style: { marginBottom: 16 } },
      `Window: ${Math.round(window.width)} × ${Math.round(window.height)} logical units · ${classes.width} / ${classes.height}`
    ),
    h(
      Label,
      { size: 14, style: { marginBottom: 16 } },
      'Available main region: ' + Math.round(width)
    ),
    h(
      AdaptiveGrid,
      { minItemWidth: 300, maxColumns: 2 },
      h(
        ExampleCard,
        { key: 'document', title: 'Document', detail: 'The selection survives layout changes.' },
        h(
          Label,
          { color: COLORS.text },
          row ? 'Selected ' + row : 'Select a row on the Documents page'
        )
      ),
      h(
        ExampleCard,
        { key: 'support', title: 'Layout details', detail: 'Actual window observations.' },
        h(Label, null, `Display scale: ${window.scale} · Font scale: ${window.fontScale}`),
        h(
          Label,
          { size: 14, style: { marginTop: 8 } },
          'This view does not simulate a hinge or qualify multiple displays.'
        )
      )
    )
  );
}

function LimitationsPage() {
  return h(
    ScrollView,
    { style: { flex: 1 }, contentContainerStyle: { paddingBottom: 16 } },
    h(
      View,
      { style: { width: '100%', maxWidth: 820 } },
      h(Heading, {
        title: 'Evidence and unsupported work',
        detail: 'Planned checks, not results from this binary or a connected Pi.',
      }),
      ...SCENARIOS.map((scenario) =>
        h(
          View,
          {
            key: scenario.id,
            style: {
              padding: 16,
              backgroundColor: COLORS.surface,
              borderRadius: 5,
              marginBottom: 12,
            },
          },
          h(
            Label,
            { color: COLORS.text },
            scenario.id + ' / ' + scenario.title + ' / ' + scenario.status
          ),
          h(Label, { size: 14, style: { marginTop: 6 } }, scenario.expectation)
        )
      )
    )
  );
}

function GalleryModal({ opener }) {
  const visible = useConformance((state) => state.modal);
  const setModal = useConformance((state) => state.setModal);
  const presses = useConformance((state) => state.presses);
  const window = useWindowDimensions();
  const closeButton = React.useRef(null);
  const wasVisible = React.useRef(false);
  // QNX does not emit iOS Modal.onDismiss. Restore only after the hidden
  // overlay commit; resize never changes the opener's React identity.
  React.useEffect(() => {
    if (visible && !wasVisible.current) closeButton.current?.focus();
    if (!visible && wasVisible.current) opener.current?.focus();
    wasVisible.current = visible;
  }, [visible, opener]);
  return h(
    Modal,
    { testID: 'gallery-modal', visible, transparent: true, onRequestClose: () => setModal(false) },
    h(
      View,
      {
        style: {
          flex: 1,
          padding: 16,
          alignItems: 'center',
          justifyContent: 'center',
          backgroundColor: COLORS.backdrop,
        },
      },
      h(
        View,
        {
          accessibilityViewIsModal: true,
          style: {
            width: '100%',
            maxWidth: 560,
            maxHeight: Math.max(0, window.height - 32),
            backgroundColor: COLORS.surface,
            borderRadius: 6,
          },
        },
        h(
          ScrollView,
          { style: { flexShrink: 1 }, contentContainerStyle: { padding: 24 } },
          h(Heading, {
            title: 'React Native Modal',
            detail: 'Return to your current task when you close this dialog.',
          }),
          h(
            Label,
            { color: COLORS.text, style: { marginBottom: 24 } },
            'Press count remains ' + presses
          ),
          h(Action, {
            ref: closeButton,
            title: 'Close modal',
            primary: true,
            onPress: () => setModal(false),
          })
        )
      )
    )
  );
}

function ConformanceApp() {
  const page = useConformance((state) => state.page);
  const visited = useConformance((state) => state.visited);
  const goTo = useConformance((state) => state.goTo);
  const setContentSize = useConformance((state) => state.setContentSize);
  const setModal = useConformance((state) => state.setModal);
  const opener = React.useRef(null);
  const openModal = (instance) => {
    opener.current = instance;
    setModal(true);
  };
  const components = {
    gallery: GalleryPage,
    list: ListPage,
    sections: SectionsPage,
    controls: ControlsPage,
    adaptive: AdaptivePage,
    limits: LimitationsPage,
  };
  return h(
    View,
    { style: { flex: 1, width: '100%', height: '100%' } },
    h(
      AdaptiveShell,
      {
        testID: 'adaptive-shell',
        title: 'Component gallery',
        subtitle: 'React Native QNX',
        navigation: PAGES,
        selectedId: page,
        onSelect: goTo,
        navigationLabel: 'Browse sections',
        onContentLayout: (event) => {
          const { width, height } = event.nativeEvent.layout;
          if (Number.isFinite(width) && Number.isFinite(height)) setContentSize(width, height);
        },
      },
      // Each visited route keeps its native subtree and full content geometry.
      // display:none would remove that subtree during Fabric mounting.
      h(
        View,
        { collapsable: false, style: { flex: 1, minHeight: 0, minWidth: 0 } },
        ...PAGES.filter((item) => visited[item.id]).map((item) =>
          h(
            View,
            {
              key: item.id,
              testID: 'gallery-route-' + item.id,
              collapsable: false,
              pointerEvents: page === item.id ? 'auto' : 'none',
              accessibilityElementsHidden: page !== item.id,
              importantForAccessibility: page === item.id ? 'auto' : 'no-hide-descendants',
              style: {
                position: 'absolute',
                top: 0,
                right: 0,
                bottom: 0,
                left: 0,
                opacity: page === item.id ? 1 : 0,
              },
            },
            h(components[item.id], { openModal })
          )
        )
      )
    ),
    h(GalleryModal, { opener })
  );
}

AppRegistry.registerComponent('RNQnxPlayground', () => ConformanceApp);
