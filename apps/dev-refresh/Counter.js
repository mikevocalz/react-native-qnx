const React = require('react');
const View = require('react-native/Libraries/Components/View/View').default;
const Text = require('react-native/Libraries/Text/Text').default;
const Pressable = require('react-native/Libraries/Components/Pressable/Pressable').default;
const DevSettings = require('react-native/Libraries/Utilities/DevSettings').default;

export default function Counter() {
  const [count, setCount] = React.useState(0);
  // Edit this label/color without changing hooks. The count must survive.
  const label = 'Edit this label';
  const color = '#203b61';
  return React.createElement(
    View,
    {
      style: { flex: 1, padding: 48, backgroundColor: color },
    },
    React.createElement(Text, { style: { color: 'white', fontSize: 32 } }, label),
    React.createElement(
      Pressable,
      {
        onPress: () => setCount((value) => value + 1),
        style: { padding: 24, marginTop: 24, backgroundColor: '#3d648e' },
      },
      React.createElement(Text, { style: { color: 'white', fontSize: 28 } }, `Count: ${count}`)
    ),
    React.createElement(
      Pressable,
      {
        onPress: () => DevSettings.reload('Fixture reload (state intentionally reset)'),
        style: { padding: 24, marginTop: 24, backgroundColor: '#3d648e' },
      },
      React.createElement(Text, { style: { color: 'white', fontSize: 24 } }, 'Reload runtime')
    )
  );
}
