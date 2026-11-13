// Bundles react-native-svg through the real QNX Metro configuration and runs
// the overlay components, to see which native component each one renders and
// with which props. `react-native` is replaced by a small stand-in, because
// its sources are Flow and need a device runtime; everything else (Metro, the
// Babel preset and its React Native codegen plugin, react-native-svg's own
// source, the overlay) is the real thing. react-test-renderer is not a
// dependency here, so components are instantiated and render() is called
// directly: each returns the one native element it stands for.
import assert from 'node:assert/strict';
import { mkdir, mkdtemp, realpath, rm, writeFile } from 'node:fs/promises';
import { createRequire } from 'node:module';
import { tmpdir } from 'node:os';
import path from 'node:path';
import vm from 'node:vm';

const root = process.cwd();
const require = createRequire(import.meta.url);
const { runBuild } = require('metro');
const { getQnxMetroConfig } = require(path.join(root, 'packages/metro-config-qnx/src/index.js'));
const rnRequire = createRequire(require.resolve('react-native/package.json'));
const svgRoot = await realpath(path.dirname(require.resolve('react-native-svg/package.json')));
const reactRoot = await realpath(path.dirname(require.resolve('react/package.json')));
const babelRuntime = await realpath(path.dirname(rnRequire.resolve('@babel/runtime/package.json')));
// Metro runs from the real path of node_modules, which sits outside `root`
// when node_modules is itself a symlink to a shared install.
const installedModules = await realpath(path.join(root, 'node_modules'));

const project = await mkdtemp(path.join(tmpdir(), 'rnqnx-svg-overlay-'));

const stub = `
'use strict';
// Enough of react-native for react-native-svg's filter, gradient and xml code.
function nativeComponent(name, config) {
  const Component = function NativeComponent() { return null; };
  Component.displayName = name;
  Component.viewConfig = config ? config() : null;
  return Component;
}
const colors = { black: 0xff000000, red: 0xffff0000, white: 0xffffffff, blue: 0xff0000ff };
module.exports = {
  // NativeComponentRegistry and the public entry share this module.
  get: nativeComponent,
  codegenNativeComponent: (name) => nativeComponent(name, null),
  // ViewConfigIgnore
  ConditionallyIgnoredEventHandlers: (handlers) => handlers,
  processColor: (value) => {
    if (typeof value === 'number') return value;
    if (typeof value === 'string' && colors[value] !== undefined) return colors[value] | 0;
    return null;
  },
  Platform: { OS: 'qnx', select: (spec) => spec.qnx ?? spec.default },
  Image: { resolveAssetSource: (source) => (typeof source === 'number' ? { uri: 'asset://' + source } : source) },
  findNodeHandle: () => null,
  StyleSheet: { create: (styles) => styles, flatten: (style) => style },
  View: function View() { return null; },
  Touchable: {},
  // extractResponder reads the key names of a PanResponder handler set.
  PanResponder: { create: () => ({ panHandlers: {} }) },
};
`;
// One literal require per module: Metro resolves them statically.
const entryCode = `
const React = require('react');
const mod = {
  tags: require('react-native-svg/src/xmlTags').tags,
  xml: require('react-native-svg/src/xml'),
  extractGradient: require('react-native-svg/src/lib/extract/extractGradient').default,
  Filter: require('react-native-svg/src/elements/filters/Filter').default,
  Text: require('react-native-svg/src/elements/Text').default,
  TSpan: require('react-native-svg/src/elements/TSpan').default,
  LinearGradient: require('react-native-svg/src/elements/LinearGradient').default,
  RadialGradient: require('react-native-svg/src/elements/RadialGradient').default,
  FeBlend: require('react-native-svg/src/elements/filters/FeBlend').default,
  FeFlood: require('react-native-svg/src/elements/filters/FeFlood').default,
  FeMerge: require('react-native-svg/src/elements/filters/FeMerge').default,
  FeMergeNode: require('react-native-svg/src/elements/filters/FeMergeNode').default,
  FeGaussianBlur: require('react-native-svg/src/elements/filters/FeGaussianBlur').default,
  FeComponentTransfer: require('react-native-svg/src/elements/filters/FeComponentTransfer').default,
  FeConvolveMatrix: require('react-native-svg/src/elements/filters/FeConvolveMatrix').default,
  FeDiffuseLighting: require('react-native-svg/src/elements/filters/FeDiffuseLighting').default,
  FeDisplacementMap: require('react-native-svg/src/elements/filters/FeDisplacementMap').default,
  FeDropShadow: require('react-native-svg/src/elements/filters/FeDropShadow').default,
  FeImage: require('react-native-svg/src/elements/filters/FeImage').default,
  FeMorphology: require('react-native-svg/src/elements/filters/FeMorphology').default,
  FeSpecularLighting: require('react-native-svg/src/elements/filters/FeSpecularLighting').default,
  FeTile: require('react-native-svg/src/elements/filters/FeTile').default,
  FeTurbulence: require('react-native-svg/src/elements/filters/FeTurbulence').default,
  FeDistantLight: require('react-native-svg/src/elements/filters/FeDistantLight').default,
  FePointLight: require('react-native-svg/src/elements/filters/FePointLight').default,
  FeSpotLight: require('react-native-svg/src/elements/filters/FeSpotLight').default,
  FeFuncA: require('react-native-svg/src/elements/filters/FeComponentTransferFunction').FeFuncA,
  FeFuncB: require('react-native-svg/src/elements/filters/FeComponentTransferFunction').FeFuncB,
  FeFuncG: require('react-native-svg/src/elements/filters/FeComponentTransferFunction').FeFuncG,
  FeFuncR: require('react-native-svg/src/elements/filters/FeComponentTransferFunction').FeFuncR,
};

function render(Component, props, givenChildren) {
  const element = React.createElement(Component, props, ...(givenChildren || []));
  const output = new Component(element.props).render();
  if (output === null) return null;
  const { children, ref: _ref, ...rest } = output.props;
  return {
    type: output.type.displayName,
    props: rest,
    // What each child primitive of a <filter> was given.
    childColorSpaces: React.Children.toArray(children).map(
      (child) => child.props.colorInterpolationFilters
    ),
    viewConfig: output.type.viewConfig,
  };
}
const el = (type, props) => React.createElement(type, props);

const results = {};
const R = (name, Component, props, children) => { results[name] = render(Component, props, children); };

R('textLength', mod.Text, { x: '0', y: '10', textLength: 120, lengthAdjust: 'spacingAndGlyphs' }, 'Hello');
R('textPlain', mod.Text, { x: '0', y: '10' }, 'Hello');
R('tspanLength', mod.TSpan, { textLength: '50%', lengthAdjust: 'spacing' }, 'Hi');
R('textBogusAdjust', mod.Text, { lengthAdjust: 'bogus' }, 'Hello');
R('textProtoAdjust', mod.Text, { lengthAdjust: 'constructor' }, 'Hello');
// Names that exist on every object must not be read as table entries.
R('imageProto', mod.FeImage, { href: '#a', preserveAspectRatio: 'constructor toString' });
R('imageProto2', mod.FeImage, { href: '#a', preserveAspectRatio: '__proto__ __proto__' });
R('linearProto', mod.LinearGradient, { id: 'g', spreadMethod: 'constructor' });
R('linearProto2', mod.LinearGradient, { id: 'g', spreadMethod: '__proto__' });
// Int32 props are clamped before they reach native.
R('turbulenceHuge', mod.FeTurbulence, { numOctaves: 1e12 });
R('turbulenceNegative', mod.FeTurbulence, { numOctaves: -5 });
R('convolveHuge', mod.FeConvolveMatrix, { order: '1e12 -1e12', targetX: 1e12, targetY: -1e12 });
// A junk entry makes the whole list unset instead of shifting the entries after it.
R('tableJunk', mod.FeComponentTransfer, {}, [el(mod.FeFuncR, { type: 'table', tableValues: '0 x 1' })]);
R('radiusJunk', mod.FeMorphology, { radius: 'x 3' });
R('convolveJunk', mod.FeConvolveMatrix, { kernelMatrix: '1 2 x 4' });
// Children are found through fragments, and by displayName.
const FakeFuncG = function FakeFuncG() { return null; };
FakeFuncG.displayName = 'FeFuncG';
const FakeLight = function FakeLight() { return null; };
FakeLight.displayName = 'FePointLight';
R('transferFragment', mod.FeComponentTransfer, {}, [
  React.createElement(React.Fragment, null,
    el(mod.FeFuncR, { type: 'discrete', tableValues: '1 0' }),
    React.createElement(React.Fragment, null, el(FakeFuncG, { type: 'linear', slope: 3 }))),
]);
R('lightFragment', mod.FeDiffuseLighting, {}, [
  React.createElement(React.Fragment, null, el(FakeLight, { x: 7, y: 8, z: 9 })),
]);
R('morphology', mod.FeMorphology, { in: 'a', operator: 'dilate', radius: '2 3', result: 'm' });
R('morphologyDefault', mod.FeMorphology, {});
R('turbulence', mod.FeTurbulence, {
  baseFrequency: '0.05 0.1', numOctaves: 3, seed: 7, stitchTiles: 'stitch', type: 'fractalNoise',
});
R('turbulenceDefault', mod.FeTurbulence, { baseFrequency: 0.02 });
R('displacement', mod.FeDisplacementMap, {
  in: 'a', in2: 'b', scale: 10, xChannelSelector: 'R', yChannelSelector: 'G',
});
R('transfer', mod.FeComponentTransfer, { in: 'a' }, [
  el(mod.FeFuncR, { type: 'table', tableValues: '0 0.5 1' }),
  el(mod.FeFuncA, { type: 'linear', slope: 0.5, intercept: 0.1 }),
  el(mod.FeFuncB, { type: 'gamma', amplitude: 2, exponent: 3, offset: 0.25 }),
]);
R('dropShadowDefault', mod.FeDropShadow, {});
R('dropShadow', mod.FeDropShadow, {
  in: 'SourceAlpha', dx: 5, dy: '-3', stdDeviation: '1 4', floodColor: 'red', floodOpacity: '50%', result: 's',
});
R('imageData', mod.FeImage, { href: 'data:image/png;base64,AAAA', preserveAspectRatio: 'xMinYMax slice' });
R('imageXlink', mod.FeImage, { xlinkHref: '#shape' });
R('imageAsset', mod.FeImage, { href: 12 });
R('convolve', mod.FeConvolveMatrix, {
  order: '3', kernelMatrix: '0 -1 0 -1 5 -1 0 -1 0', divisor: 1, bias: 0.1, targetX: 1, targetY: 1,
  edgeMode: 'wrap', preserveAlpha: 'true',
});
R('convolveDefault', mod.FeConvolveMatrix, {});
// Values that are not numbers fall back to the defaults instead of reaching the host.
R('convolveHostile', mod.FeConvolveMatrix, {
  order: 'x', kernelMatrix: 'NaN Infinity 2', divisor: 'abc', targetX: Infinity, bias: -Infinity,
});
R('turbulenceHostile', mod.FeTurbulence, { baseFrequency: [NaN], numOctaves: NaN, seed: Infinity });
R('morphologyHostile', mod.FeMorphology, { radius: 'NaN' });
R('lightHostile', mod.FeDiffuseLighting, { surfaceScale: 'x', diffuseConstant: NaN }, [
  el(mod.FeSpotLight, { x: NaN, limitingConeAngle: 'abc' }),
]);
R('tile', mod.FeTile, { in: 't', x: 1, y: 2, width: 30, height: 40 });
R('diffuseDistant', mod.FeDiffuseLighting, { in: 'a', surfaceScale: 2, diffuseConstant: 0.5, lightingColor: 'red' }, [
  el(mod.FeDistantLight, { azimuth: 45, elevation: 60 }),
]);
R('diffusePoint', mod.FeDiffuseLighting, {}, [el(mod.FePointLight, { x: 1, y: 2, z: 3 })]);
R('diffuseNoLight', mod.FeDiffuseLighting, {});
R('specularSpot', mod.FeSpecularLighting, { specularConstant: 1.5, specularExponent: 20, lightingColor: 'currentColor' }, [
  el(mod.FeSpotLight, { x: 1, y: 2, z: 3, pointsAtX: 4, pointsAtY: 5, pointsAtZ: 6, specularExponent: 2, limitingConeAngle: 30 }),
]);
R('specularSpotNoCone', mod.FeSpecularLighting, {}, [el(mod.FeSpotLight, { x: 1 })]);

R('blurSrgb', mod.FeBlend, { in: 'a', in2: 'b', mode: 'multiply', colorInterpolationFilters: 'sRGB' });
R('blurDefault', mod.FeBlend, {});
// SvgXml turns numeric-looking attributes into numbers; names must stay strings.
R('blendNumericNames', mod.FeBlend, { in: 1, in2: 2, result: 3 });
R('mergeNumericNames', mod.FeMerge, { result: 4 }, [
  el(mod.FeMergeNode, { in: 5 }),
  el(mod.FeMergeNode, {}),
]);
R('floodAttribute', mod.FeFlood, { floodColor: 'red', floodOpacity: 0.25 });
R('floodStyle', mod.FeFlood, {
  floodColor: 'black',
  style: { floodColor: 'blue', floodOpacity: '0.5' },
});
R('floodDefault', mod.FeFlood, {});
R('blurStyle', mod.FeGaussianBlur, { stdDeviation: 2, style: { colorInterpolationFilters: 'linearRGB' } });
R('filterSrgb', mod.Filter, { id: 'f', colorInterpolationFilters: 'sRGB' }, [
  el(mod.FeBlend, {}),
  el(mod.FeBlend, { colorInterpolationFilters: 'linearRGB' }),
  el(mod.FeBlend, { colorInterpolationFilters: 'auto' }),
]);
R('filterAuto', mod.Filter, { id: 'f', colorInterpolationFilters: 'auto' }, [el(mod.FeBlend, {})]);
R('filterDefault', mod.Filter, { id: 'f' }, [el(mod.FeBlend, {})]);

R('linearRepeat', mod.LinearGradient, { id: 'g', spreadMethod: 'repeat' });
R('linearDefault', mod.LinearGradient, { id: 'g' });
R('radialReflect', mod.RadialGradient, { id: 'g', spreadMethod: 'reflect' }, [
  React.createElement('stop', { offset: 0.5, stopColor: 'red' }),
]);
results.gradientNoId = mod.extractGradient({}, null);

// SvgXml's parser: the element table and a feComponentTransfer round trip.
const ast = mod.xml.parse(
  '<svg><filter id="f" color-interpolation-filters="sRGB"><feComponentTransfer in="SourceGraphic">' +
    '<feFuncR type="table" tableValues="0 1"/><feFuncG type="discrete" tableValues="1 0"/>' +
    '</feComponentTransfer></filter></svg>'
);
const filterElement = ast.children[0];
const transferElement = filterElement.props.children[0];
results.xml = {
  tagNames: ['feFuncR', 'feFuncG', 'feFuncB', 'feFuncA', 'feMergeNode', 'feDropShadow'].filter(
    (name) => mod.tags[name] !== undefined
  ),
  filter: render(filterElement.type, filterElement.props),
  transfer: render(transferElement.type, transferElement.props),
};

globalThis.overlayResults = JSON.stringify(results, (key, value) => (value === undefined ? '__undefined__' : value));
`;

/**
 * Bundles the probe for `platform` and returns its results.
 * @param {string} platform
 */
async function run(platform) {
  const stubPath = path.join(project, 'react-native-stub.js');
  const baseConfig = getQnxMetroConfig(project, {
    maxWorkers: 2,
    watchFolders: [project, svgRoot, reactRoot, babelRuntime, installedModules, root],
    resolver: {
      extraNodeModules: {
        react: reactRoot,
        'react-native-svg': svgRoot,
        '@babel/runtime': babelRuntime,
      },
      nodeModulesPaths: [],
    },
    serializer: { getModulesRunBeforeMainModule: () => [], getPolyfills: () => [] },
    reporter: { update() {} },
  });
  const next = baseConfig.resolver.resolveRequest;
  const config = {
    ...baseConfig,
    resolver: {
      ...baseConfig.resolver,
      /**
       * @param {any} context
       * @param {string} moduleName
       * @param {string | null} requestedPlatform
       */
      resolveRequest(context, moduleName, requestedPlatform) {
        if (moduleName === 'react-native' || moduleName.startsWith('react-native/')) {
          return { type: 'sourceFile', filePath: stubPath };
        }
        return next(context, moduleName, requestedPlatform);
      },
    },
  };
  const bundle = await runBuild(config, {
    entry: path.join(project, 'probe.js'),
    platform,
    dev: false,
    minify: false,
  });
  /** @type {{ console: object, overlayResults?: string }} */
  const sandbox = { console: { warn() {}, error() {}, log() {} } };
  vm.runInNewContext(bundle.code, sandbox);
  return JSON.parse(String(sandbox.overlayResults), (key, value) =>
    value === '__undefined__' ? undefined : value
  );
}

try {
  await writeFile(
    path.join(project, 'package.json'),
    JSON.stringify({ name: 'svg-overlay-probe' })
  );
  await writeFile(
    path.join(project, 'babel.config.js'),
    `module.exports = { presets: [${JSON.stringify(require.resolve('babel-preset-expo'))}] };\n`
  );
  await writeFile(path.join(project, 'react-native-stub.js'), stub);
  await mkdir(project, { recursive: true });
  await writeFile(path.join(project, 'probe.js'), entryCode);

  const qnx = await run('qnx');
  const props = (/** @type {string} */ name) => {
    assert.ok(qnx[name], `${name} rendered nothing on qnx`);
    return qnx[name].props;
  };
  const native = (/** @type {string} */ name, /** @type {string} */ type) =>
    assert.equal(qnx[name]?.type, type, `${name} renders ${type}`);

  // Primitives the package never rendered.
  native('morphology', 'RNSVGFeMorphology');
  assert.deepEqual(props('morphology'), {
    in1: 'a',
    operator1: 'dilate',
    radiusX: 2,
    radiusY: 3,
    result: 'm',
  });
  assert.deepEqual(props('morphologyDefault'), { radiusX: 0, radiusY: 0 });

  native('turbulence', 'RNSVGFeTurbulence');
  assert.deepEqual(props('turbulence'), {
    baseFrequencyX: 0.05,
    baseFrequencyY: 0.1,
    numOctaves: 3,
    seed: 7,
    stitchTiles: 'stitch',
    type: 'fractalNoise',
  });
  assert.deepEqual(props('turbulenceDefault'), {
    baseFrequencyX: 0.02,
    baseFrequencyY: 0.02,
    numOctaves: 1,
    seed: 0,
  });

  native('displacement', 'RNSVGFeDisplacementMap');
  assert.deepEqual(props('displacement'), {
    in1: 'a',
    in2: 'b',
    scale: 10,
    xChannelSelector: 'R',
    yChannelSelector: 'G',
  });

  native('transfer', 'RNSVGFeComponentTransfer');
  const identity = {
    type: 'identity',
    tableValues: [],
    slope: 1,
    intercept: 0,
    amplitude: 1,
    exponent: 1,
    offset: 0,
  };
  assert.deepEqual(props('transfer'), {
    in1: 'a',
    funcR: { ...identity, type: 'table', tableValues: [0, 0.5, 1] },
    funcA: { ...identity, type: 'linear', slope: 0.5, intercept: 0.1 },
    funcB: { ...identity, type: 'gamma', amplitude: 2, exponent: 3, offset: 0.25 },
  });

  native('dropShadowDefault', 'RNSVGFeDropShadow');
  assert.deepEqual(props('dropShadowDefault'), {
    dx: 2,
    dy: 2,
    stdDeviationX: 2,
    stdDeviationY: 2,
  });
  assert.deepEqual(props('dropShadow'), {
    in1: 'SourceAlpha',
    dx: 5,
    dy: '-3',
    stdDeviationX: 1,
    stdDeviationY: 4,
    floodColor: { type: 0, payload: -65536 },
    floodOpacity: 0.5,
    result: 's',
  });

  native('imageData', 'RNSVGFeImage');
  assert.deepEqual(props('imageData'), {
    href: 'data:image/png;base64,AAAA',
    align: 'xMinYMax',
    meetOrSlice: 1,
  });
  assert.deepEqual(props('imageXlink'), { href: '#shape', align: 'xMidYMid', meetOrSlice: 0 });
  assert.equal(props('imageAsset').href, 'asset://12');

  native('convolve', 'RNSVGFeConvolveMatrix');
  assert.deepEqual(props('convolve'), {
    orderX: 3,
    orderY: 3,
    kernelMatrix: [0, -1, 0, -1, 5, -1, 0, -1, 0],
    divisor: 1,
    bias: 0.1,
    targetX: 1,
    targetY: 1,
    edgeMode: 'wrap',
    preserveAlpha: true,
  });
  assert.deepEqual(props('convolveDefault'), {
    orderX: 3,
    orderY: 3,
    kernelMatrix: [],
    divisor: 0,
    bias: 0,
    targetX: -1,
    targetY: -1,
    preserveAlpha: false,
  });

  assert.deepEqual(props('convolveHostile'), {
    orderX: 3,
    orderY: 3,
    kernelMatrix: [],
    divisor: 0,
    bias: 0,
    targetX: -1,
    targetY: -1,
    preserveAlpha: false,
  });
  assert.deepEqual(props('turbulenceHostile'), {
    baseFrequencyX: 0,
    baseFrequencyY: 0,
    numOctaves: 1,
    seed: 0,
  });
  assert.deepEqual(props('morphologyHostile'), { radiusX: 0, radiusY: 0 });
  assert.equal(props('lightHostile').surfaceScale, 1);
  assert.equal(props('lightHostile').diffuseConstant, 1);
  assert.equal(props('lightHostile').light.x, 0);
  assert.equal(props('lightHostile').light.hasLimitingCone, false);
  native('tile', 'RNSVGFeTile');
  assert.deepEqual(props('tile'), { in1: 't', x: 1, y: 2, width: 30, height: 40 });

  native('diffuseDistant', 'RNSVGFeDiffuseLighting');
  assert.deepEqual(props('diffuseDistant'), {
    in1: 'a',
    surfaceScale: 2,
    diffuseConstant: 0.5,
    lightingColor: { type: 0, payload: -65536 },
    light: { kind: 'distant', azimuth: 45, elevation: 60 },
  });
  assert.deepEqual(props('diffusePoint').light, { kind: 'point', x: 1, y: 2, z: 3 });
  assert.deepEqual(props('diffuseNoLight').light, { kind: 'none' });

  native('specularSpot', 'RNSVGFeSpecularLighting');
  assert.deepEqual(props('specularSpot'), {
    surfaceScale: 1,
    specularConstant: 1.5,
    specularExponent: 20,
    lightingColor: { type: 2 },
    light: {
      kind: 'spot',
      x: 1,
      y: 2,
      z: 3,
      pointsAtX: 4,
      pointsAtY: 5,
      pointsAtZ: 6,
      specularExponent: 2,
      limitingConeAngle: 30,
      hasLimitingCone: true,
    },
  });
  assert.equal(props('specularSpotNoCone').light.hasLimitingCone, false);

  // color-interpolation-filters reaches the native props of the package's own primitives.
  native('blurSrgb', 'RNSVGFeBlend');
  assert.equal(props('blurSrgb').colorInterpolationFilters, 'sRGB');
  assert.equal(props('blurDefault').colorInterpolationFilters, undefined);
  assert.deepEqual(props('blendNumericNames'), {
    in1: '1',
    in2: '2',
    result: '3',
    mode: 'normal',
  });
  native('floodAttribute', 'RNSVGFeFlood');
  assert.deepEqual(props('floodAttribute'), {
    floodColor: { type: 0, payload: -65536 },
    floodOpacity: 0.25,
  });
  assert.deepEqual(props('floodStyle'), {
    floodColor: { type: 0, payload: -16776961 },
    floodOpacity: 0.5,
  });
  assert.deepEqual(props('floodDefault'), {
    floodColor: { type: 0, payload: -16777216 },
    floodOpacity: 1,
  });
  native('mergeNumericNames', 'RNSVGFeMerge');
  assert.deepEqual(props('mergeNumericNames'), { result: '4', nodes: ['5', ''] });
  assert.equal(props('blurStyle').colorInterpolationFilters, 'linearRGB');
  native('filterSrgb', 'RNSVGFilter');
  // A <filter>'s value is inherited by the primitives that have none of their own.
  assert.deepEqual(qnx.filterSrgb.childColorSpaces, ['sRGB', 'linearRGB', 'sRGB']);
  assert.deepEqual(Array.from(qnx.filterAuto.childColorSpaces), [undefined]);
  assert.deepEqual(Array.from(qnx.filterDefault.childColorSpaces), [undefined]);

  // textLength and lengthAdjust, which the package's extractText drops.
  native('textLength', 'RNSVGText');
  assert.equal(props('textLength').textLength, 120);
  assert.equal(props('textLength').lengthAdjust, 'spacingAndGlyphs');
  assert.equal(props('textPlain').textLength, undefined);
  assert.equal(props('textPlain').lengthAdjust, undefined);
  native('tspanLength', 'RNSVGTSpan');
  assert.equal(props('tspanLength').textLength, '50%');
  assert.equal(props('tspanLength').lengthAdjust, 'spacing');

  native('textBogusAdjust', 'RNSVGText');
  assert.equal(props('textBogusAdjust').lengthAdjust, undefined);
  assert.equal(props('textProtoAdjust').lengthAdjust, undefined);

  assert.deepEqual(props('imageProto'), { href: '#a', align: 'xMidYMid', meetOrSlice: 0 });
  assert.deepEqual(props('imageProto2'), { href: '#a', align: 'xMidYMid', meetOrSlice: 0 });
  assert.equal(props('linearProto').spreadMethod, 0);
  assert.equal(props('linearProto2').spreadMethod, 0);

  assert.equal(props('turbulenceHuge').numOctaves, 24);
  assert.equal(props('turbulenceNegative').numOctaves, 0);
  assert.deepEqual([props('convolveHuge').orderX, props('convolveHuge').orderY], [32, 0]);
  assert.deepEqual([props('convolveHuge').targetX, props('convolveHuge').targetY], [32, -1]);

  assert.deepEqual(props('tableJunk').funcR.tableValues, []);
  assert.deepEqual(props('radiusJunk'), { radiusX: 0, radiusY: 0 });
  assert.deepEqual(props('convolveJunk').kernelMatrix, []);

  assert.deepEqual(props('transferFragment').funcR.tableValues, [1, 0]);
  assert.equal(props('transferFragment').funcG.type, 'linear');
  assert.equal(props('transferFragment').funcG.slope, 3);
  assert.deepEqual(props('lightFragment').light, { kind: 'point', x: 7, y: 8, z: 9 });

  // spreadMethod.
  native('linearRepeat', 'RNSVGLinearGradient');
  assert.equal(props('linearRepeat').spreadMethod, 2);
  assert.equal(props('linearRepeat').name, 'g');
  assert.equal(props('linearDefault').spreadMethod, 0);
  assert.equal(props('radialReflect').spreadMethod, 1);
  assert.deepEqual(props('radialReflect').gradient, [0.5, -65536]);
  assert.equal(qnx.gradientNoId, null);

  // SvgXml and its relatives build elements from the package's tag table.
  assert.deepEqual(qnx.xml.tagNames, [
    'feFuncR',
    'feFuncG',
    'feFuncB',
    'feFuncA',
    'feMergeNode',
    'feDropShadow',
  ]);
  assert.deepEqual(qnx.xml.filter.childColorSpaces, ['sRGB']);
  assert.equal(qnx.xml.transfer.type, 'RNSVGFeComponentTransfer');
  assert.deepEqual(qnx.xml.transfer.props.funcR, {
    ...identity,
    type: 'table',
    tableValues: [0, 1],
  });
  assert.deepEqual(qnx.xml.transfer.props.funcG, {
    ...identity,
    type: 'discrete',
    tableValues: [1, 0],
  });
  assert.equal(qnx.xml.transfer.props.in1, 'SourceGraphic');

  // The JavaScript view config is what React Native checks props against: a
  // prop it does not list is dropped before it reaches the host.
  const attributes = (/** @type {string} */ name) => {
    const config = qnx[name].viewConfig;
    assert.ok(config, `${name} has a view config`);
    return Object.keys(config.validAttributes);
  };
  for (const [name, expectedProps] of Object.entries({
    morphology: ['in1', 'operator1', 'radiusX', 'radiusY', 'colorInterpolationFilters', 'result'],
    turbulence: ['baseFrequencyX', 'baseFrequencyY', 'numOctaves', 'seed', 'stitchTiles', 'type'],
    displacement: ['in1', 'in2', 'scale', 'xChannelSelector', 'yChannelSelector'],
    transfer: ['funcR', 'funcG', 'funcB', 'funcA'],
    dropShadow: ['dx', 'dy', 'stdDeviationX', 'stdDeviationY', 'floodColor', 'floodOpacity'],
    imageData: ['href', 'align', 'meetOrSlice'],
    convolve: [
      'orderX',
      'orderY',
      'kernelMatrix',
      'divisor',
      'bias',
      'targetX',
      'targetY',
      'edgeMode',
      'preserveAlpha',
    ],
    tile: ['in1'],
    diffuseDistant: ['surfaceScale', 'diffuseConstant', 'lightingColor', 'light'],
    specularSpot: [
      'surfaceScale',
      'specularConstant',
      'specularExponent',
      'lightingColor',
      'light',
    ],
    blurSrgb: ['in1', 'in2', 'mode', 'colorInterpolationFilters'],
    linearRepeat: ['gradient', 'gradientUnits', 'gradientTransform', 'spreadMethod'],
    radialReflect: ['spreadMethod'],
  })) {
    const listed = attributes(name);
    for (const prop of expectedProps)
      assert.ok(listed.includes(prop), `${name} view config lists ${prop}`);
  }

  // Off QNX the package behaves as it always did.
  const ios = await run('ios');
  assert.equal(ios.morphology, null, 'FeMorphology still renders nothing on ios');
  assert.equal(ios.transfer, null);
  assert.equal(ios.blurSrgb.props.colorInterpolationFilters, undefined);
  assert.equal(ios.linearRepeat.props.spreadMethod, undefined);
  assert.deepEqual(ios.xml.tagNames, ['feMergeNode', 'feDropShadow']);

  console.log(
    `react-native-svg overlay: ${Object.keys(qnx).length} qnx probes checked, ${Object.keys(ios).length} ios probes untouched`
  );
} finally {
  await rm(project, { recursive: true, force: true });
}
