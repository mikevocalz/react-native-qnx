// The react-native-svg fixture corpus (tests/fixtures/rnsvg) drawn through
// react-native-svg 15.16.0's own SvgXml, the real Fabric props and the Qt
// bridge, for tests/native/qt-rnsvg-fabric/RnsvgCorpusFabricTest.cpp.
//
// The SVG documents come from a module that `scripts/rnsvg-reference.mjs
// --corpus` writes at build time (rnsvg-corpus-generated/rnsvg-corpus-data.js):
// batches of fixed cells, each with the document text and its position. The
// test picks a batch with __rnsvgCorpusSetBatch(index), waits for the cells to
// mount, grabs the window and diffs every cell against its resvg golden.
const React = require('react');
const { useStore } = require('zustand');
const { createStore } = require('zustand/vanilla');
const AppRegistry = require('react-native/Libraries/ReactNative/AppRegistry').AppRegistry;
const View = require('react-native/Libraries/Components/View/View').default;
const { SvgXml } = require('react-native-svg');
// SvgXml leaves <style> blocks alone; SvgCss applies them, which the fixtures
// marked `css` in the manifest rely on.
const { SvgCss } = require('react-native-svg/css');
const data = require('./rnsvg-corpus-generated/rnsvg-corpus-data');

globalThis.__rnsvgCorpusBatchCount = data.batches.length;
globalThis.__rnsvgCorpusRenders = 0;

function Corpus() {
  const store = React.useMemo(() => createStore(() => ({ batch: -1 })), []);
  const batch = useStore(store, (state) => state.batch);
  globalThis.__rnsvgCorpusSetBatch = (index) => store.setState({ batch: index });
  globalThis.__rnsvgCorpusRenders++;
  const cells = batch >= 0 && batch < data.batches.length ? data.batches[batch] : [];
  return (
    <>
      {cells.map((cell) => {
        const Root = cell.css ? SvgCss : SvgXml;
        return (
          <View
            key={batch + ':' + cell.name}
            style={{
              position: 'absolute',
              left: cell.x,
              top: cell.y,
              width: cell.w,
              height: cell.h,
            }}
          >
            {/* width and height override the document's own, as an app's layout would. */}
            <Root
              xml={cell.xml}
              width={cell.w}
              height={cell.h}
              onError={(error) => console.error(cell.name + ': ' + error.message)}
            />
          </View>
        );
      })}
    </>
  );
}

AppRegistry.registerComponent('RnqnxRnsvgCorpus', () => Corpus);
