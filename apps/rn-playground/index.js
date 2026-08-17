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
const Animated = require('react-native/Libraries/Animated/Animated').default;
const AppRegistry = require('react-native/Libraries/ReactNative/AppRegistry').AppRegistry;
const View = require('react-native/Libraries/Components/View/View').default;
const Text = require('react-native/Libraries/Text/Text').default;
const Image = require('react-native/Libraries/Image/Image').default;
const ImageBackground = require('react-native/Libraries/Image/ImageBackground').default;
const ScrollView = require('react-native/Libraries/Components/ScrollView/ScrollView').default;
const Pressable = require('react-native/Libraries/Components/Pressable/Pressable').default;
const FlatList = require('react-native/Libraries/Lists/FlatList').default;
const SectionList = require('react-native/Libraries/Lists/SectionList').default;
// React Native's TextInput.js branches on Platform.OS and has no third
// branch, so on qnx it renders nothing. This uses the QNX platform override
// that translates the public `value`/`defaultValue`/`onChangeText` surface to
// the native single-line textinput descriptor.
const TextInput = require('react-native/Libraries/Components/TextInput/TextInput').default;
const ActivityIndicator =
  require('react-native/Libraries/Components/ActivityIndicator/ActivityIndicator').default;
const Switch = require('react-native/Libraries/Components/Switch/Switch').default;
const SafeAreaView = require('react-native/Libraries/Components/SafeAreaView/SafeAreaView').default;
const Button = require('react-native/Libraries/Components/Button').default;
const Modal = require('react-native/Libraries/Modal/Modal').default;

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
          globalThis.__lastScrollContentHeight = event.nativeEvent.contentSize.height;
        },
        onScrollBeginDrag: () => {
          globalThis.__scrollBeginDrag = (globalThis.__scrollBeginDrag ?? 0) + 1;
        },
        onScrollEndDrag: () => {
          globalThis.__scrollEndDrag = (globalThis.__scrollEndDrag ?? 0) + 1;
        },
        onMomentumScrollBegin: () => {
          globalThis.__momentumScrollBegin = (globalThis.__momentumScrollBegin ?? 0) + 1;
        },
        onMomentumScrollEnd: () => {
          globalThis.__momentumScrollEnd = (globalThis.__momentumScrollEnd ?? 0) + 1;
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

/**
 * FlatList is upstream JavaScript over ScrollView, so it is registered
 * separately: it exercises virtualisation rather than the mounting path the
 * component tests index into.
 */
function FlatListApp() {
  const data = [];
  for (let i = 0; i < 20; i++) {
    data.push({ key: String(i) });
  }
  return React.createElement(
    View,
    { style: { width: 200, height: 100 }, collapsable: false },
    React.createElement(FlatList, {
      style: { width: 200, height: 100 },
      data,
      initialNumToRender: 4,
      // The default windowSize of 21 viewports covers 2100pt, far more than
      // 20 rows of 30pt, so the list would legitimately render everything and
      // prove nothing. One viewport is what makes windowing observable.
      windowSize: 1,
      onScroll: (event) => {
        globalThis.__listScrollY = event.nativeEvent.contentOffset.y;
      },
      onLayout: (event) => {
        globalThis.__listViewportH = event.nativeEvent.layout.height;
      },
      onContentSizeChange: (w, h) => {
        globalThis.__listContentH = h;
      },
      renderItem: ({ item }) =>
        React.createElement(View, {
          style: { width: 200, height: 30, backgroundColor: '#3366cc' },
          collapsable: false,
          // VirtualizedList sizes cells from onLayout when getItemLayout is
          // absent, so this records whether that reaches JavaScript at all.
          onLayout: (event) => {
            globalThis.__cellLayouts = (globalThis.__cellLayouts ?? 0) + 1;
            globalThis.__lastCellHeight = event.nativeEvent.layout.height;
          },
        }),
    })
  );
}

function TextEllipsisApp() {
  return React.createElement(
    View,
    {
      style: { width: 200, height: 40 },
      collapsable: false,
    },
    React.createElement(
      Text,
      {
        style: { fontSize: 16 },
        numberOfLines: 1,
        ellipsizeMode: 'tail',
      },
      'A very long string that should be truncated with an ellipsis'
    )
  );
}

function TextInputMultilineApp() {
  return React.createElement(
    View,
    { style: { width: 200, height: 80 }, collapsable: false },
    React.createElement(TextInput, {
      style: { width: 200, height: 80, fontSize: 16 },
      collapsable: false,
      multiline: true,
      defaultValue: 'line one',
      placeholder: 'type multiple lines',
      placeholderTextColor: '#ff0000',
      onChangeText: (text) => {
        globalThis.__multilineText = text;
      },
    })
  );
}

function TextInputApp() {
  return React.createElement(
    View,
    { style: { width: 200, height: 60 }, collapsable: false },
    React.createElement(TextInput, {
      style: { width: 200, height: 40, fontSize: 18 },
      collapsable: false,
      defaultValue: 'typed',
      placeholder: 'enter text',
      onFocus: (event) => {
        globalThis.__inputFocus = (globalThis.__inputFocus ?? 0) + 1;
        globalThis.__inputFocusText = event.nativeEvent.text;
      },
      onBlur: (event) => {
        globalThis.__inputBlur = (globalThis.__inputBlur ?? 0) + 1;
        globalThis.__inputBlurText = event.nativeEvent.text;
      },
      onChange: (event) => {
        globalThis.__inputChanges = (globalThis.__inputChanges ?? 0) + 1;
        globalThis.__inputText = event.nativeEvent.text;
      },
      onChangeText: (text) => {
        globalThis.__inputChangeText = (globalThis.__inputChangeText ?? 0) + 1;
        globalThis.__inputTextFromChangeText = text;
      },
      onKeyPress: (event) => {
        globalThis.__inputKeyPress = (globalThis.__inputKeyPress ?? 0) + 1;
        globalThis.__inputLastKey = event.nativeEvent.key;
      },
      onSubmitEditing: (event) => {
        globalThis.__inputSubmit = (globalThis.__inputSubmit ?? 0) + 1;
        globalThis.__inputSubmitText = event.nativeEvent.text;
      },
      onSelectionChange: (event) => {
        globalThis.__inputSelection = event.nativeEvent.selection;
      },
    })
  );
}

function ActivityIndicatorApp() {
  return React.createElement(
    View,
    { style: { width: 200, height: 200, backgroundColor: '#eeeeee' }, collapsable: false },
    React.createElement(ActivityIndicator, {
      size: 'large',
      color: '#00aa66',
      collapsable: false,
      animating: true,
    })
  );
}

function SwitchApp() {
  const valueState = React.useState(false);
  const value = valueState[0];
  const setValue = valueState[1];
  return React.createElement(
    View,
    { style: { width: 200, height: 60 }, collapsable: false },
    React.createElement(Switch, {
      value,
      onValueChange: (newValue) => {
        globalThis.__switchValueChange = (globalThis.__switchValueChange ?? 0) + 1;
        globalThis.__switchLastValue = newValue;
        setValue(newValue);
      },
      trackColor: { false: '#767577', true: '#81b0ff' },
      thumbColor: '#f4f3f4',
    })
  );
}

function SafeAreaViewApp() {
  return React.createElement(
    SafeAreaView,
    { style: { flex: 1, backgroundColor: '#336699' }, collapsable: false },
    React.createElement(View, {
      style: { width: 50, height: 50, backgroundColor: '#ff0000' },
      collapsable: false,
    })
  );
}

function ModalApp() {
  return React.createElement(
    Modal,
    {
      visible: true,
      transparent: false,
      collapsable: false,
      style: { flex: 1, backgroundColor: '#336699' },
    },
    React.createElement(Text, { style: { color: '#ffffff' } }, 'Modal')
  );
}

function ImageBackgroundApp() {
  const source =
    'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4nGMwTpv5HwAENAIyWy0K4AAAAABJRU5ErkJggg==';
  return React.createElement(
    ImageBackground,
    {
      style: { width: 64, height: 64 },
      imageStyle: { width: 64, height: 64 },
      source: { uri: source },
      resizeMode: 'cover',
    },
    React.createElement(Text, { style: { color: '#ffffff' } }, 'Background')
  );
}

function TransformApp() {
  return React.createElement(View, {
    style: {
      width: 100,
      height: 50,
      left: 50,
      top: 60,
      backgroundColor: '#0000ff',
      transform: [{ translateX: 20 }, { translateY: 10 }, { rotate: '45deg' }, { scale: 1.5 }],
    },
    collapsable: false,
  });
}

function ViewLayoutApp() {
  return React.createElement(
    View,
    {
      style: { width: 120, height: 60, backgroundColor: '#00ff00' },
      collapsable: false,
      onLayout: (event) => {
        globalThis.__viewLayout = event.nativeEvent.layout;
      },
    },
    React.createElement(View, { style: { width: 20, height: 20 } })
  );
}

function ImageDefaultApp() {
  const fallback =
    'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4nGMwTpv5HwAENAIyWy0K4AAAAABJRU5ErkJggg==';
  return React.createElement(Image, {
    style: { width: 32, height: 32 },
    resizeMode: 'contain',
    defaultSource: { uri: fallback },
    onLoad: (event) => {
      globalThis.__imageDefaultLoad = (globalThis.__imageDefaultLoad ?? 0) + 1;
      globalThis.__imageDefaultUri = event.nativeEvent.source.uri;
    },
  });
}

function ImageLoadingApp() {
  const primary =
    'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4nGMwTpv5HwAENAIyWy0K4AAAAABJRU5ErkJggg==';
  const loading =
    'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR4nGNgYAAAAAMAASsJ7XAAAAAASUVORK5CYII=';
  return React.createElement(Image, {
    style: { width: 32, height: 32 },
    resizeMode: 'contain',
    source: { uri: primary },
    loadingIndicatorSource: { uri: loading },
    onLoad: (event) => {
      globalThis.__imageLoadingPrimaryLoad =
        (globalThis.__imageLoadingPrimaryLoad ?? 0) + 1;
      globalThis.__imageLoadingPrimaryUri = event.nativeEvent.source.uri;
    },
    onError: () => {
      globalThis.__imageLoadingError =
        (globalThis.__imageLoadingError ?? 0) + 1;
    },
  });
}

function ImageGetSizeApp() {
  const Image = require('react-native/Libraries/Image/Image.ios').default;
  const source =
    'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4nGMwTpv5HwAENAIyWy0K4AAAAABJRU5ErkJggg==';
  Image.getSize(source)
    .then(({ width, height }) => {
      globalThis.__imageGetSize = { width, height };
    })
    .catch((e) => {
      globalThis.__imageGetSizeError = e.message;
    });
  return null;
}

function ImageResolveAssetApp() {
  const Image = require('react-native/Libraries/Image/Image').default;
  globalThis.__imageAssetResolved = Image.resolveAssetSource(require('./qnx-asset.png'));
  return React.createElement(View, { style: { width: 1, height: 1 }, collapsable: false }, null);
}

function SectionListApp() {
  const sections = [
    { title: 'A', data: ['one', 'two'] },
    { title: 'B', data: ['three'] },
  ];
  return React.createElement(SectionList, {
    style: { width: 200, height: 120 },
    collapsable: false,
    sections,
    keyExtractor: (item) => item,
    renderSectionHeader: ({ section }) =>
      React.createElement(Text, { style: { fontSize: 18 } }, section.title),
    renderItem: ({ item }) => React.createElement(Text, { style: { fontSize: 14 } }, item),
    onViewableItemsChanged: ({ viewableItems }) => {
      globalThis.__sectionListViewable = viewableItems.length;
    },
  });
}

function NativeAnimatedApp() {
  const opacity = React.useRef(new Animated.Value(0.5)).current;
  return React.createElement(
    Animated.View,
    {
      style: {
        width: 100,
        height: 50,
        backgroundColor: '#ff0000',
        opacity,
      },
      collapsable: false,
    },
    null
  );
}

function ButtonApp() {
  return React.createElement(Button, {
    title: 'OK',
    onPress: () => {
      globalThis.__buttonPress = (globalThis.__buttonPress ?? 0) + 1;
    },
  });
}

function ImageApp() {
  const source =
    'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4nGMwTpv5HwAENAIyWy0K4AAAAABJRU5ErkJggg==';
  return React.createElement(Image, {
    style: { width: 32, height: 32 },
    resizeMode: 'contain',
    source: { uri: source },
    onLoadStart: () => {
      globalThis.__imageLoadStart = (globalThis.__imageLoadStart ?? 0) + 1;
    },
    onLoad: (event) => {
      globalThis.__imageLoad = (globalThis.__imageLoad ?? 0) + 1;
      globalThis.__imageLoadUri = event.nativeEvent.source.uri;
    },
    onLoadEnd: () => {
      globalThis.__imageLoadEnd = (globalThis.__imageLoadEnd ?? 0) + 1;
    },
    onError: () => {
      globalThis.__imageError = (globalThis.__imageError ?? 0) + 1;
    },
  });
}

AppRegistry.registerComponent('RNQnxPlayground', () => PlaygroundApp);
AppRegistry.registerComponent('RNQnxViewLayout', () => ViewLayoutApp);
AppRegistry.registerComponent('RNQnxTransform', () => TransformApp);
AppRegistry.registerComponent('RNQnxFlatList', () => FlatListApp);
AppRegistry.registerComponent('RNQnxSectionList', () => SectionListApp);
AppRegistry.registerComponent('RNQnxTextEllipsis', () => TextEllipsisApp);
AppRegistry.registerComponent('RNQnxTextInput', () => TextInputApp);
AppRegistry.registerComponent('RNQnxTextInputMultiline', () => TextInputMultilineApp);
AppRegistry.registerComponent('RNQnxActivityIndicator', () => ActivityIndicatorApp);
AppRegistry.registerComponent('RNQnxSwitch', () => SwitchApp);
AppRegistry.registerComponent('RNQnxSafeAreaView', () => SafeAreaViewApp);
AppRegistry.registerComponent('RNQnxImageBackground', () => ImageBackgroundApp);
AppRegistry.registerComponent('RNQnxImage', () => ImageApp);
AppRegistry.registerComponent('RNQnxImageDefault', () => ImageDefaultApp);
AppRegistry.registerComponent('RNQnxImageLoading', () => ImageLoadingApp);
AppRegistry.registerComponent('RNQnxImageGetSize', () => ImageGetSizeApp);
AppRegistry.registerComponent('RNQnxImageResolveAsset', () => ImageResolveAssetApp);
AppRegistry.registerComponent('RNQnxNativeAnimated', () => NativeAnimatedApp);
AppRegistry.registerComponent('RNQnxButton', () => ButtonApp);
AppRegistry.registerComponent('RNQnxModal', () => ModalApp);
