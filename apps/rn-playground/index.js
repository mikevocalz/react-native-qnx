/**
 * The smallest entry that exercises the real render path.
 *
 * It deliberately does not `import 'react-native'`: that entry pulls in the
 * TurboModule registry and a long tail of native modules this platform has not
 * implemented, which would fail for reasons that have nothing to do with
 * rendering. Importing AppRegistry and View directly keeps the bundle to what
 * the renderer actually needs.
 */

const React = require('react');
const AppRegistry = require('react-native/Libraries/ReactNative/AppRegistry').AppRegistry;
const View = require('react-native/Libraries/Components/View/View').default;
const Text = require('react-native/Libraries/Text/Text').default;
const Image = require('react-native/Libraries/Image/Image').default;
const ScrollView = require('react-native/Libraries/Components/ScrollView/ScrollView').default;
const Pressable = require('react-native/Libraries/Components/Pressable/Pressable').default;

function PlaygroundApp() {
  return React.createElement(
    View,
    {
      style: {
        width: 320,
        height: 180,
        backgroundColor: '#204080',
        borderRadius: 12,
      },
      collapsable: false,
    },
    React.createElement(View, {
      style: {
        width: 64,
        height: 32,
        backgroundColor: '#ff8800',
        opacity: 0.5,
      },
      collapsable: false,
      // The host test presses here and reads these back, which is how a touch
      // reaching JavaScript is proven rather than assumed.
      onTouchStart: (event) => {
        globalThis.__touchStarts = (globalThis.__touchStarts ?? 0) + 1;
        globalThis.__lastTouch = event.nativeEvent.locationX;
      },
      onTouchEnd: () => {
        globalThis.__touchEnds = (globalThis.__touchEnds ?? 0) + 1;
      },
      onStartShouldSetResponder: () => {
        globalThis.__askedResponder = (globalThis.__askedResponder ?? 0) + 1;
        return true;
      },
      onResponderGrant: () => {
        globalThis.__granted = (globalThis.__granted ?? 0) + 1;
      },
      onResponderRelease: () => {
        globalThis.__released = (globalThis.__released ?? 0) + 1;
      },
    }),
    // Pressable sits on top of the responder system. It is the first component
    // this platform renders that is not a bare View.
    React.createElement(Pressable, {
      style: { width: 64, height: 32, backgroundColor: '#00aa66' },
      collapsable: false,
      onPress: () => {
        globalThis.__presses = (globalThis.__presses ?? 0) + 1;
      },
    }),
    // Text is measured by the platform's TextLayoutManager, so its size comes
    // from Qt's font metrics rather than from an explicit style.
    React.createElement(Text, { style: { fontSize: 24 }, collapsable: false }, 'Hello QNX'),
    // Two nested spans with different styles, which Qt Quick's Text can only
    // express as rich text.
    React.createElement(
      Text,
      { style: { fontSize: 14 }, collapsable: false },
      'plain ',
      React.createElement(Text, { style: { fontSize: 20, color: '#ffcc00' } }, 'styled')
    ),
    // Long enough to wrap inside the 320pt parent, so the measured height has
    // to account for more than one line.
    React.createElement(
      Text,
      { style: { fontSize: 16 }, collapsable: false },
      'This sentence is deliberately long enough that it must wrap onto more ' +
        'than one line inside a three hundred and twenty point wide parent.'
    ),
    // A 2x2 magenta PNG inlined as a data URI, so the playground needs no
    // network or file access to prove the image path.
    // A ScrollView whose content is taller than its frame, so the Flickable
    // has something to scroll.
    React.createElement(
      ScrollView,
      {
        // flexShrink: 0 is deliberate. React Native's ScrollView base style is
        // shrinkable, and this parent's children already overflow it, so Yoga
        // collapsed the scroll view to zero height without it.
        style: { width: 100, height: 50, flexShrink: 0 },
        collapsable: false,
        // The host test scrolls the Qt Flickable directly and reads these
        // back, which is how a scroll reaching JavaScript is proven.
        onScroll: (event) => {
          globalThis.__scrolls = (globalThis.__scrolls ?? 0) + 1;
          globalThis.__lastScrollY = event.nativeEvent.contentOffset.y;
          globalThis.__lastScrollContentHeight =
            event.nativeEvent.contentSize.height;
        },
        scrollEventThrottle: 16,
      },
      React.createElement(View, {
        style: { width: 100, height: 300, backgroundColor: '#8800ff' },
        collapsable: false,
      })
    ),
    React.createElement(Image, {
      style: { width: 40, height: 24 },
      collapsable: false,
      source: {
        uri: 'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAIAAAD91JpzAAAAEElEQVR4nGP4z/AfiBggFAA31gf5btK+fQAAAABJRU5ErkJggg==',
      },
    })
  );
}

AppRegistry.registerComponent('RNQnxPlayground', () => PlaygroundApp);
