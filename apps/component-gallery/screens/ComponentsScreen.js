// Components tab: the basic React Native cards.
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
const FastRefreshCard = require('../FastRefreshCard').default;
const RemoteImageCard = require('../RemoteImageCard').default;
const ExpoModulesCard = require('../ExpoModulesCard').default;
const ExpoImageCard = require('../ExpoImageCard').default;
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
  // The page is the root's content width; the root pads 28 px on each side.
  const inner = Math.max(0, width - 2 * 28);
  const columns = gridColumns(inner);
  const cardWidth = Math.floor((inner - GAP * (columns - 1)) / columns);
  let cardIndex = 0;
  const card = (key, title, detail, ...children) => {
    const lastInRow = cardIndex % columns === columns - 1;
    cardIndex += 1;
    return h(
      View,
      {
        key,
        style: {
          width: inner > 0 ? cardWidth : '100%',
          padding: 22,
          marginBottom: GAP,
          marginRight: lastInRow ? 0 : GAP,
          minHeight: 236,
          borderRadius: 14,
          backgroundColor: PANEL,
        },
      },
      h(Heading, { title, detail }),
      ...children
    );
  };
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
        'fast-refresh',
        'Fast Refresh',
        __DEV__ ? 'Live from Metro on your computer' : 'Bundled bytecode build',
        h(FastRefreshCard)
      ),
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
            source: require('../assets/icon.png'),
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
        'expo-modules',
        'Expo modules',
        'expo-constants through the QNX Expo host',
        h(ExpoModulesCard)
      ),
      card(
        'expo-image',
        'expo-image',
        'Native ExpoImage view on the Qt image loader',
        h(ExpoImageCard)
      ),
      card(
        'remote-image',
        'Image from a URL',
        'HTTP(S) through Qt Network on the device',
        h(RemoteImageCard)
      ),
      card(
        'press',
        'Pressable',
        'Mouse/touch → React handler → native text update',
        h(ActionButton, { title: 'Press count: ' + presses, onPress: press }),
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
            backgroundColor: BACKGROUND,
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
            trackColor: { false: '#3a4d63', true: '#2fb5a8' },
            thumbColor: '#f4f8fb',
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
        h(ActionButton, { title: 'Open modal', onPress: () => setModal(true) }),
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
          h(ActionButton, { title: 'Close modal', onPress: () => setModal(false) })
        )
      )
    )
  );
}

module.exports = { ComponentsScreen: GalleryPage };
