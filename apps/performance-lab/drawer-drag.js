// Fixed workload, not a benchmark result. The drawer is the shipped JsDrawer
// (packages/expo-router-qnx/src/JsDrawer.js): a PanResponder edge strip and an
// Animated.Value that follows the pointer. Nothing here sets the drawer
// position. The timing probe, or a person, drags from the left edge with
// pointer events, and the responder system decides whether that is a drag.
const React = require('react');
const AppRegistry = require('react-native/Libraries/ReactNative/AppRegistry').AppRegistry;
const View = require('react-native/Libraries/Components/View/View').default;
const Text = require('react-native/Libraries/Text/Text').default;
const { JsDrawer } = require('../../packages/expo-router-qnx/src/JsDrawer');

const h = React.createElement;
const ROWS = Array.from({ length: 12 }, (_, i) => 'Drawer entry ' + String(i + 1).padStart(2, '0'));

function DrawerDrag() {
  const [open, setOpen] = React.useState(false);
  return h(
    View,
    {
      collapsable: false,
      style: { flex: 1, width: '100%', height: '100%', backgroundColor: '#102033' },
    },
    h(
      JsDrawer,
      {
        open,
        onOpen: () => setOpen(true),
        onClose: () => setOpen(false),
        drawerWidth: 280,
        layout: { width: 800, height: 600 },
        drawerStyle: { backgroundColor: '#23505b' },
        renderDrawerContent: () =>
          h(
            View,
            { collapsable: false, style: { flex: 1, padding: 16 } },
            ROWS.map((label) =>
              h(
                Text,
                { key: label, style: { color: '#e3edf7', fontSize: 16, paddingVertical: 8 } },
                label
              )
            )
          ),
      },
      h(
        View,
        { collapsable: false, style: { flex: 1, padding: 24, backgroundColor: '#233341' } },
        h(
          Text,
          { style: { color: '#ffffff', fontSize: 18 } },
          'RNQNX performance workload / drawer-drag'
        ),
        h(
          Text,
          { style: { color: '#9dafc2', fontSize: 14, marginTop: 8 } },
          open ? 'open' : 'closed'
        )
      )
    )
  );
}

AppRegistry.registerComponent('RNQnxPlayground', () => DrawerDrag);
