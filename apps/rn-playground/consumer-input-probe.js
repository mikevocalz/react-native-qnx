// Real consumer-window input regression. No synthetic JS handler invocation.
const React = require('react');
const AppRegistry = require('react-native/Libraries/ReactNative/AppRegistry').AppRegistry;
const View = require('react-native/Libraries/Components/View/View').default;
const Text = require('react-native/Libraries/Text/Text').default;
const Pressable = require('react-native/Libraries/Components/Pressable/Pressable').default;
const Modal = require('react-native/Libraries/Modal/Modal').default;
const ScrollView = require('react-native/Libraries/Components/ScrollView/ScrollView').default;
const Switch = require('react-native/Libraries/Components/Switch/Switch').default;
const h = React.createElement;

function ConsumerInputProbe() {
  const pressState = React.useState(0);
  const presses = pressState[0];
  const setPresses = pressState[1];
  const pageState = React.useState('Components');
  const page = pageState[0];
  const setPage = pageState[1];
  const disabledState = React.useState(0);
  const disabledPresses = disabledState[0];
  const setDisabledPresses = disabledState[1];
  const modalState = React.useState(false);
  const modal = modalState[0];
  const setModal = modalState[1];
  const switchState = React.useState(false);
  const modalSwitchState = React.useState(false);
  const button = (title, left, top, onPress, disabled = false) =>
    h(
      Pressable,
      {
        disabled,
        onPress,
        style: {
          position: 'absolute',
          left,
          top,
          width: 180,
          height: 80,
          padding: 20,
          borderRadius: 12,
          backgroundColor: '#276478',
        },
      },
      h(Text, { style: { color: '#ffffff', fontSize: 16 } }, title)
    );
  const modalContent = h(
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
        { style: { width: 280, height: 180, padding: 20, backgroundColor: '#142538' } },
        h(Text, { style: { color: '#ffffff' } }, 'Consumer modal open'),
        h(Switch, {
          value: modalSwitchState[0],
          onValueChange: modalSwitchState[1],
          style: { position: 'absolute', left: 20, top: 45 },
        }),
        h(
          Text,
          { style: { position: 'absolute', left: 90, top: 50, color: '#ffffff' } },
          'Modal switch: ' + (modalSwitchState[0] ? 'on' : 'off')
        ),
        h(
          Pressable,
          {
            onPress: () => setModal(false),
            style: {
              position: 'absolute',
              left: 20,
              top: 90,
              width: 240,
              height: 60,
              padding: 20,
              backgroundColor: '#276478',
            },
          },
          h(Text, { style: { color: '#ffffff' } }, 'Close modal')
        )
      )
    )
  );
  return h(
    View,
    { style: { flex: 1, backgroundColor: '#142538' } },
    button('Press me', 20, 20, () => setPresses((value) => value + 1)),
    button('Documents', 220, 20, () => setPage('Documents')),
    button('Disabled', 20, 120, () => setDisabledPresses((value) => value + 1), true),
    button('Open modal', 220, 120, () => setModal(true)),
    h(
      Text,
      { style: { position: 'absolute', left: 20, top: 220, color: '#ffffff' } },
      'Consumer presses: ' + presses
    ),
    h(
      Text,
      { style: { position: 'absolute', left: 20, top: 250, color: '#ffffff' } },
      'Consumer page: ' + page
    ),
    h(
      Text,
      { style: { position: 'absolute', left: 20, top: 280, color: '#ffffff' } },
      'Disabled presses: ' + disabledPresses
    ),
    h(Switch, {
      value: switchState[0],
      onValueChange: switchState[1],
      style: { position: 'absolute', left: 20, top: 320 },
    }),
    h(
      Text,
      { style: { position: 'absolute', left: 20, top: 370, color: '#ffffff' } },
      'Native switch: ' + (switchState[0] ? 'on' : 'off')
    ),
    h(
      ScrollView,
      { style: { position: 'absolute', left: 430, top: 20, width: 180, height: 300 } },
      h(
        View,
        { style: { width: 180, height: 900 } },
        h(Text, { style: { color: '#ffffff' } }, 'Scroll before opening'),
        h(
          View,
          { style: { position: 'absolute', left: 15, top: 400, width: 150, height: 200 } },
          h(Text, { style: { color: '#ffffff' } }, 'Scrolled modal owner'),
          modalContent
        )
      )
    )
  );
}

AppRegistry.registerComponent('RNQnxPlayground', () => ConsumerInputProbe);
