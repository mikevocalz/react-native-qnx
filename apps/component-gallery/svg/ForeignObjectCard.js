const React = require('react');
const { View, Text, Pressable } = require('react-native');
const Svg = require('react-native-svg').default;
const { G, Rect, ForeignObject } = require('react-native-svg');
const { useStore } = require('zustand');
const { createStore } = require('zustand/vanilla');
const { SvgCardFrame } = require('./SvgCardFrame');
const { INK, MUTED, SELECTED } = require('./theme');

function ForeignObjectCard() {
  const store = React.useMemo(() => createStore(() => ({ presses: 0, blocked: false })), []);
  const presses = useStore(store, (state) => state.presses);
  const blocked = useStore(store, (state) => state.blocked);
  const increment = React.useCallback(
    () => store.setState((state) => ({ presses: state.presses + 1 })),
    [store]
  );
  const toggle = React.useCallback(
    () => store.setState((state) => ({ blocked: !state.blocked })),
    [store]
  );
  return (
    <SvgCardFrame
      title="ForeignObject live children"
      detail="A transformed SVG group contains a live React Native button; ancestor pointerEvents=none blocks its input"
    >
      <Svg width="100%" height={170} viewBox="0 0 560 170">
        <G transform="translate(40 15)" pointerEvents={blocked ? 'none' : 'auto'}>
          <Rect width="240" height="140" rx="12" fill="#1b5b61" />
          <ForeignObject x="10" y="10" width="220" height="120">
            <View style={{ width: 220, height: 120, backgroundColor: '#79e8dc', padding: 10 }}>
              <Pressable
                onPress={increment}
                accessibilityLabel="Press live SVG child"
                style={{
                  height: 80,
                  justifyContent: 'center',
                  alignItems: 'center',
                  backgroundColor: '#142538',
                }}
              >
                <Text style={{ color: INK, fontSize: 16 }}>Live presses: {presses}</Text>
              </Pressable>
            </View>
          </ForeignObject>
        </G>
      </Svg>
      <Text style={{ color: MUTED, fontSize: 14 }}>
        Input {blocked ? 'blocked' : 'enabled'}; count {presses}
      </Text>
      <Pressable
        onPress={toggle}
        accessibilityLabel="Toggle SVG child input"
        style={{ backgroundColor: SELECTED, padding: 10, alignSelf: 'flex-start', marginTop: 8 }}
      >
        <Text style={{ color: INK }}>{blocked ? 'Enable child input' : 'Block child input'}</Text>
      </Pressable>
    </SvgCardFrame>
  );
}

module.exports = {
  id: 'svg-foreign-object',
  title: 'ForeignObject live children',
  Component: ForeignObjectCard,
};
