// react-native-svg 15.16.0 <Image> elements, mounted through the production
// Fabric -> Qt path, for tests/native/qt-rnsvg-fabric/RnsvgImageFabricTest.cpp.
// No JSX: this file sits outside apps/rn-playground, whose Babel config the
// Metro build picks by working directory.
const React = require('react');
const { useState } = React;
const AppRegistry = require('react-native/Libraries/ReactNative/AppRegistry').AppRegistry;
const RNImage = require('react-native/Libraries/Image/Image').default;
const Svg = require('react-native-svg').default;
const { Image } = require('react-native-svg');

// 2x2 PNGs. FIRST: red, green / blue, yellow. SECOND: black, white / white, black.
const FIRST =
  'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAIAAAD91JpzAAAAFElEQVR42mP4z8DAAMIM////ZwAAHu8E/HMcU8wAAAAASUVORK5CYII=';
const SECOND =
  'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAIAAAD91JpzAAAAE0lEQVR42mNgYGD4//8/GDMwAAAp5AX7YWIVfgAAAABJRU5ErkJggg==';
// Not a PNG: the load fails and nothing is drawn or reported.
const BROKEN = 'data:image/png;base64,AAAA';

// A 4x4 PNG, magenta, cyan / orange, violet quadrants. The test copies it to
// the path Image.resolveAssetSource reports, because `metro build` does not
// write assets.
const ASSET = require('./rnsvg-image-asset.png');
globalThis.__rnsvgImageAssetUri = RNImage.resolveAssetSource(ASSET).uri;
globalThis.__rnsvgImageFirst = FIRST;

globalThis.__rnsvgImageLoads = { data: 0, asset: 0, broken: 0 };
globalThis.__rnsvgImageLast = { data: '', asset: '', broken: '' };

function loaded(name) {
  return (event) => {
    globalThis.__rnsvgImageLoads[name]++;
    globalThis.__rnsvgImageLast[name] = JSON.stringify(event.nativeEvent.source);
  };
}

function Fixture() {
  const [state, setState] = useState({ dataHref: FIRST, width: 200 });
  globalThis.__rnsvgImageUpdate = (patch) => setState((previous) => ({ ...previous, ...patch }));
  const e = React.createElement;
  return e(
    Svg,
    { width: state.width, height: 100, viewBox: '0 0 200 100', style: { position: 'absolute', left: 0, top: 0 } },
    e(Image, {
      x: 0, y: 0, width: 50, height: 50, preserveAspectRatio: 'none',
      href: state.dataHref === 'second' ? SECOND : FIRST,
      onLoad: loaded('data'),
    }),
    e(Image, {
      x: 100, y: 0, width: 50, height: 50, preserveAspectRatio: 'none',
      href: ASSET,
      onLoad: loaded('asset'),
    }),
    e(Image, {
      x: 0, y: 50, width: 50, height: 50, preserveAspectRatio: 'none',
      href: BROKEN,
      onLoad: loaded('broken'),
    })
  );
}

AppRegistry.registerComponent('RnqnxRnsvgImageFixture', () => Fixture);
