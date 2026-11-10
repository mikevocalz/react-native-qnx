// Real consumer-window input regression. No synthetic JS handler invocation.
const React = require('react');
const AppRegistry = require('react-native/Libraries/ReactNative/AppRegistry').AppRegistry;
const View = require('react-native/Libraries/Components/View/View').default;
const Text = require('react-native/Libraries/Text/Text').default;
const Pressable = require('react-native/Libraries/Components/Pressable/Pressable').default;
const Modal = require('react-native/Libraries/Modal/Modal').default;
const ScrollView = require('react-native/Libraries/Components/ScrollView/ScrollView').default;
const Switch = require('react-native/Libraries/Components/Switch/Switch').default;
const TextInput = require('react-native/Libraries/Components/TextInput/TextInput').default;
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
  const backgroundText = React.useState('');
  const modalText = React.useState('');
  const secondModalText = React.useState('');
  const nested = React.useState(false);
  const nestedText = React.useState('');
  const closeRequests = React.useState(0);
  const nestedCloseRequests = React.useState(0);
  const rejectedEvents = React.useState(0);
  const deferredValue = React.useState(false);
  const deferredProposal = React.useState(false);
  const deferredEvents = React.useState(0);
  const field = (value, onChangeText, placeholder, style) =>
    h(TextInput, {
      value,
      onChangeText,
      placeholder,
      style: Object.assign(
        { width: 180, height: 32, color: '#ffffff', backgroundColor: '#276478' },
        style
      ),
    });
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
    {
      visible: modal,
      transparent: true,
      onRequestClose: () => {
        closeRequests[1]((value) => value + 1);
        setModal(false);
      },
    },
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
      field(modalText[0], modalText[1], 'Modal first input', {
        position: 'absolute',
        right: 20,
        top: 20,
      }),
      field(secondModalText[0], secondModalText[1], 'Modal second input', {
        position: 'absolute',
        right: 20,
        top: 60,
      }),
      h(
        Text,
        { style: { position: 'absolute', right: 20, top: 100, color: '#ffffff' } },
        'Modal text: ' + modalText[0] + ' / ' + secondModalText[0]
      ),
      h(
        Pressable,
        {
          onPress: () => nested[1](true),
          style: {
            position: 'absolute',
            right: 20,
            top: 130,
            width: 180,
            height: 48,
            backgroundColor: '#276478',
          },
        },
        h(Text, { style: { color: '#ffffff' } }, 'Open nested modal')
      ),
      h(
        Modal,
        {
          visible: modal && nested[0],
          transparent: true,
          onRequestClose: () => {
            nestedCloseRequests[1]((value) => value + 1);
            nested[1](false);
          },
        },
        h(
          View,
          {
            style: {
              flex: 1,
              alignItems: 'center',
              justifyContent: 'center',
              backgroundColor: 'rgba(0,0,0,0.85)',
            },
          },
          h(Text, { style: { color: '#ffffff' } }, 'Nested modal open'),
          field(nestedText[0], nestedText[1], 'Nested modal input'),
          h(
            Pressable,
            { onPress: () => nested[1](false), style: { padding: 20, backgroundColor: '#276478' } },
            h(Text, { style: { color: '#ffffff' } }, 'Close nested modal')
          )
        )
      ),
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
    h(Switch, {
      value: false,
      onValueChange: () => rejectedEvents[1]((value) => value + 1),
      style: { position: 'absolute', left: 220, top: 320 },
    }),
    h(
      Text,
      { style: { position: 'absolute', left: 220, top: 370, color: '#ffffff' } },
      'Rejected switch events: ' + rejectedEvents[0]
    ),
    h(Switch, {
      value: deferredValue[0],
      onValueChange: (value) => {
        deferredProposal[1](value);
        deferredEvents[1]((count) => count + 1);
      },
      style: { position: 'absolute', left: 300, top: 320 },
    }),
    field(backgroundText[0], backgroundText[1], 'Background input', {
      position: 'absolute',
      left: 20,
      top: 410,
    }),
    h(
      Text,
      { style: { position: 'absolute', left: 20, top: 450, color: '#ffffff' } },
      'Background text: ' + backgroundText[0]
    ),
    button('Apply deferred switch', 220, 410, () => deferredValue[1](deferredProposal[0])),
    h(
      Text,
      { style: { position: 'absolute', left: 20, top: 500, color: '#ffffff' } },
      'Deferred switch: ' + deferredValue[0] + ' events: ' + deferredEvents[0]
    ),
    h(
      Text,
      { style: { position: 'absolute', left: 20, top: 530, color: '#ffffff' } },
      'Modal requests: ' + closeRequests[0] + ' nested: ' + nestedCloseRequests[0]
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
