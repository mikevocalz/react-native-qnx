// Controls tab: controlled inputs.
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
      h(Switch, {
        value: toggle,
        onValueChange: setToggle,
        trackColor: { false: '#3a4d63', true: '#2fb5a8' },
        thumbColor: '#f4f8fb',
      }),
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

module.exports = { ControlsScreen: ControlsPage };
