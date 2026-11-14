const React = require('react');
const { Pressable, Text, View } = require('react-native');
const { createStore } = require('zustand/vanilla');
const { useStore } = require('zustand');
const { JsDrawer } = require('@rn-qnx/expo-router-qnx/src/JsDrawer');

function DrawerProof() {
  const store = React.useMemo(
    () => createStore(() => ({ open: false, ignored: false, requests: 0 })),
    []
  );
  const open = useStore(store, (state) => state.open);
  const ignored = useStore(store, (state) => state.ignored);
  const requests = useStore(store, (state) => state.requests);
  const pending = React.useRef(null);
  React.useEffect(() => () => clearTimeout(pending.current), []);
  const request = React.useCallback(
    (next) => {
      clearTimeout(pending.current);
      store.setState((state) => ({ requests: state.requests + 1 }));
      if (!store.getState().ignored) {
        pending.current = setTimeout(() => store.setState({ open: next }), 200);
      }
    },
    [store]
  );
  const text = { color: '#e9f0f7', fontSize: 13 };
  const button = { backgroundColor: '#1b5b61', padding: 9, marginBottom: 8 };
  return (
    <View>
      <Pressable
        style={button}
        onPress={() => store.setState((state) => ({ ignored: !state.ignored }))}
      >
        <Text style={text}>Parent {ignored ? 'ignores requests' : 'confirms after 200ms'}</Text>
      </Pressable>
      <Text style={text}>
        Controlled: {open ? 'open' : 'closed'}; requests {requests}
      </Text>
      <View style={{ width: 400, height: 220, overflow: 'hidden', marginTop: 8 }}>
        <JsDrawer
          open={open}
          onOpen={() => request(true)}
          onClose={() => request(false)}
          drawerWidth={200}
          layout={{ width: 400, height: 220 }}
          drawerStyle={{ backgroundColor: '#23505b' }}
          renderDrawerContent={() => (
            <View style={{ flex: 1, padding: 16 }}>
              <Text style={text}>Drawer panel</Text>
              <Pressable style={[button, { marginTop: 12 }]} onPress={() => request(false)}>
                <Text style={text}>Close panel</Text>
              </Pressable>
            </View>
          )}
        >
          <View style={{ flex: 1, padding: 30, backgroundColor: '#233341' }}>
            <Text style={text}>Drag from this view's left edge to open.</Text>
            <Text style={text}>Drag the panel left to close.</Text>
          </View>
        </JsDrawer>
      </View>
    </View>
  );
}
module.exports = { DrawerProof };
