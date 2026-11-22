// @ts-nocheck: Babel and native host-element stand-ins for actual example screens.
// This exercises app handlers/state/layout; Router navigation and native rendering
// are not simulated as passing here. Production Metro bundling is a separate check.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import Module, { createRequire } from 'node:module';
import path from 'node:path';

const require = createRequire(import.meta.url);
const React = require('react');
const { act } = React;
const TestRenderer = require('react-test-renderer');
const babel = require('@babel/core');
const root = path.resolve('apps/expo-router-example');
const starterRoot = path.resolve('template/qnx-router');
let windowWidth = 400;
globalThis.IS_REACT_ACT_ENVIRONMENT = true;
const host = (name) => (props) => React.createElement(name, props);
const Tabs = host('Tabs');
Tabs.Screen = host('TabScreen');
const originalLoad = Module._load;
const originalJs = Module._extensions['.js'];
Module._load = function (request, parent, isMain) {
  if (request === 'react-native')
    return {
      View: host('View'),
      Text: host('Text'),
      TextInput: host('TextInput'),
      ScrollView: host('ScrollView'),
      Pressable: host('Pressable'),
      StyleSheet: { create: (styles) => styles },
      useWindowDimensions: () => ({ width: windowWidth, height: 800 }),
    };
  if (request === 'expo-router') return { Tabs, Link: host('Link') };
  if (request === '@rn-qnx/expo-router-qnx')
    return originalLoad.call(
      this,
      path.resolve('packages/expo-router-qnx/src/index.js'),
      parent,
      isMain
    );
  return originalLoad.call(this, request, parent, isMain);
};
Module._extensions['.js'] = function (module, filename) {
  if (
    (!filename.startsWith(root + path.sep) && !filename.startsWith(starterRoot + path.sep)) ||
    filename.includes('node_modules')
  )
    return originalJs(module, filename);
  const result = babel.transformSync(fs.readFileSync(filename, 'utf8'), {
    filename,
    configFile: false,
    babelrc: false,
    plugins: ['@babel/plugin-transform-modules-commonjs', '@babel/plugin-transform-react-jsx'],
  });
  module._compile(result.code, filename);
};

try {
  const { ExampleProvider } = require(path.join(root, 'components/example-store.js'));
  const Draft = require(path.join(root, 'app/index.js')).default;
  const Review = require(path.join(root, 'app/review.js')).default;
  const Layout = require(path.join(root, 'app/_layout.js')).default;
  const h = React.createElement;
  let renderer;
  const tree = (page) => h(ExampleProvider, null, h(page));
  act(() => {
    renderer = TestRenderer.create(tree(Draft));
  });
  const byId = (id) =>
    renderer.root.findAll((node) => typeof node.type === 'string' && node.props.testID === id)[0];
  act(() => {
    byId('draft-note').props.onChangeText('A note kept across routes');
    byId('add-visit').props.onPress();
    byId('add-visit').props.onPress();
  });
  assert.equal(byId('draft-note').props.value, 'A note kept across routes');
  assert.equal(renderer.root.findByType('Link').props.href, '/review');
  act(() => {
    renderer.update(tree(Review));
  });
  assert.equal(byId('review-note').props.children, 'A note kept across routes');
  assert.deepEqual(byId('review-visits').props.children, ['Visits: ', 2]);
  assert.equal(renderer.root.findByType('Link').props.href, '/');
  act(() => {
    byId('reset-draft').props.onPress();
  });
  assert.equal(byId('review-note').props.children, 'Your draft is empty.');
  act(() => {
    renderer.update(tree(Draft));
  });
  assert.equal(byId('draft-note').props.value, '');
  act(() => {
    byId('draft-note').props.onChangeText('First app only');
  });
  let other;
  act(() => {
    other = TestRenderer.create(tree(Draft));
  });
  assert.equal(
    other.root.findByType('TextInput').props.value,
    '',
    'separate roots do not share drafts'
  );
  assert.equal(byId('draft-note').props.value, 'First app only');
  act(() => {
    renderer.unmount();
  });
  act(() => {
    renderer = TestRenderer.create(tree(Draft));
  });
  assert.equal(byId('draft-note').props.value, '', 'closed roots do not retain serialized state');
  act(() => {
    renderer.unmount();
    other.unmount();
  });

  act(() => {
    renderer = TestRenderer.create(h(Layout));
  });
  assert.equal(renderer.root.findByType('Tabs').props.screenOptions.tabBarPosition, 'bottom');
  assert.deepEqual(
    renderer.root.findAllByType('TabScreen').map((node) => node.props.name),
    ['index', 'review']
  );
  windowWidth = 1100;
  act(() => {
    renderer.update(h(Layout));
  });
  assert.equal(renderer.root.findByType('Tabs').props.screenOptions.tabBarPosition, 'left');
  act(() => {
    renderer.unmount();
  });
  const StarterLayout = require(path.join(starterRoot, 'app/(tabs)/_layout.js')).default;
  windowWidth = 420;
  act(() => {
    renderer = TestRenderer.create(h(StarterLayout));
  });
  assert.equal(renderer.root.findByType('Tabs').props.screenOptions.tabBarPosition, 'bottom');
  windowWidth = 1280;
  act(() => {
    renderer.update(h(StarterLayout));
  });
  assert.equal(renderer.root.findByType('Tabs').props.screenOptions.tabBarPosition, 'left');
  act(() => {
    renderer.unmount();
  });
  console.log(
    'Router example: actual screen editing, route remount retention, reset, root isolation and adaptive tabs pass (host stand-ins only).'
  );
} finally {
  Module._load = originalLoad;
  Module._extensions['.js'] = originalJs;
}
