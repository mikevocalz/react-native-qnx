// @ts-nocheck: Execute the gallery component with controlled router/native imports.
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { createRequire } from 'node:module';
import vm from 'node:vm';

const require = createRequire(import.meta.url);
const React = require('react');
const babel = require('@babel/core');
const source = readFileSync('apps/component-gallery/components/DrawerContent.js', 'utf8');
const { code } = babel.transformSync(source, {
  babelrc: false,
  configFile: false,
  plugins: [
    require.resolve('@babel/plugin-transform-modules-commonjs'),
    require.resolve('@babel/plugin-transform-react-jsx'),
  ],
});
const navigated = [];
let pathname = '/';
const exported = {};
vm.runInNewContext(code, {
  exports: exported,
  require(name) {
    if (name === 'react') return React;
    if (name === 'react-native') return { Pressable: 'Pressable', Text: 'Text', View: 'View' };
    if (name === 'expo-router')
      return {
        useRouter: () => ({ navigate: (href) => navigated.push(href) }),
        usePathname: () => pathname,
      };
    if (name === 'expo-constants') return { expoConfig: { name: 'Gallery' } };
    if (name === '../ui') return { SELECTED: '#selected' };
    throw new Error(name);
  },
});
const routes = ['(tabs)', 'components', 'media', 'settings', 'about'].map((name) => ({ name }));
let closed = 0;
function rows(index, child) {
  pathname = child;
  const tree = exported.default({
    state: { index, routes },
    navigation: { closeDrawer: () => closed++ },
  });
  return React.Children.toArray(tree.props.children).filter((node) => node.props.item);
}
function check(index, child) {
  const items = rows(index, child);
  const selected = items.filter((row) => row.props.active);
  assert.equal(selected.length, 1, `${child}: exactly one drawer destination`);
  assert.equal(
    selected[0].props.item.id,
    ['home', 'components', 'media', 'settings', 'about'][index]
  );
  for (const row of items) {
    const pressable = row.type(row.props);
    assert.equal(pressable.props.accessibilityState.selected, row.props.active);
    assert.equal(
      pressable.props.style({}).backgroundColor,
      row.props.active ? '#selected' : 'transparent'
    );
  }
}
for (const child of ['/', '/advanced', '/documents', '/controls', '/more']) check(0, child);
for (let index = 1; index < routes.length; index++) check(index, '/' + routes[index].name);
// The navigator state remains authoritative during a URL update and on return
// to an already mounted child tab. Closing/reopening does not alter selection.
check(0, '/settings');
check(3, '/controls');
check(0, '/controls');
const media = rows(0, '/controls').find((row) => row.props.item.id === 'media');
media.props.onPress();
assert.deepEqual(navigated, ['/media']);
assert.equal(closed, 1);
check(2, '/media');
check(0, '/controls');
console.log(
  'Gallery drawer: active navigator destination survives child tabs, route changes and reopen.'
);
