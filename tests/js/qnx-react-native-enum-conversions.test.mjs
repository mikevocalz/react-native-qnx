import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { mkdtemp, readFile, rm, writeFile } from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';

const renderer = new URL(
  '../../node_modules/react-native/ReactCommon/react/renderer/',
  import.meta.url
);
/** @typedef {{file: string, primitives: string, type: string, signature: string, mutation?: boolean, cases: [string, string | number][]}} Conversion */
/** @type {Conversion[]} */
const conversions = [
  {
    file: 'components/view/accessibilityPropsConversions.h',
    primitives: 'components/view/AccessibilityPrimitives.h',
    type: 'ImportantForAccessibility',
    signature:
      'inline std::string toString(const ImportantForAccessibility &importantForAccessibility)',
    cases: [
      ['Auto', 'auto'],
      ['Yes', 'yes'],
      ['No', 'no'],
      ['NoHideDescendants', 'no-hide-descendants'],
    ],
  },
  {
    file: 'components/view/accessibilityPropsConversions.h',
    primitives: 'components/view/AccessibilityPrimitives.h',
    type: 'AccessibilityLiveRegion',
    signature: 'inline std::string toString(AccessibilityLiveRegion accessibilityLiveRegion)',
    cases: [
      ['None', 'none'],
      ['Polite', 'polite'],
      ['Assertive', 'assertive'],
    ],
  },
  {
    file: 'components/view/accessibilityPropsConversions.h',
    primitives: 'components/view/AccessibilityPrimitives.h',
    type: 'AccessibilityState::CheckedState',
    signature: 'inline std::string toString(AccessibilityState::CheckedState state)',
    cases: [
      ['Unchecked', 'Unchecked'],
      ['Checked', 'Checked'],
      ['Mixed', 'Mixed'],
      ['None', 'None'],
    ],
  },
  {
    file: 'components/view/conversions.h',
    primitives: 'components/view/primitives.h',
    type: 'PointerEventsMode',
    signature: 'inline std::string toString(PointerEventsMode value)',
    cases: [
      ['Auto', 'auto'],
      ['None', 'none'],
      ['BoxNone', 'box-none'],
      ['BoxOnly', 'box-only'],
    ],
  },
  {
    file: 'components/view/conversions.h',
    primitives: 'components/view/primitives.h',
    type: 'LayoutConformance',
    signature: 'inline std::string toString(const LayoutConformance &value)',
    cases: [
      ['Strict', 'strict'],
      ['Compatibility', 'compatibility'],
    ],
  },
  {
    file: 'core/conversions.h',
    primitives: 'core/LayoutPrimitives.h',
    type: 'LayoutDirection',
    signature: 'inline std::string toString(const LayoutDirection &layoutDirection)',
    cases: [
      ['Undefined', 'undefined'],
      ['LeftToRight', 'ltr'],
      ['RightToLeft', 'rtl'],
    ],
  },
  {
    file: 'core/conversions.h',
    primitives: 'core/LayoutPrimitives.h',
    type: 'LayoutDirection',
    signature: 'inline int toInt(const LayoutDirection &layoutDirection)',
    cases: [
      ['Undefined', 0],
      ['LeftToRight', 1],
      ['RightToLeft', 2],
    ],
  },
  {
    file: 'core/conversions.h',
    primitives: 'core/LayoutPrimitives.h',
    type: 'DisplayType',
    signature: 'inline std::string toString(const DisplayType &displayType)',
    cases: [
      ['None', 'none'],
      ['Flex', 'flex'],
      ['Contents', 'contents'],
      ['Grid', 'grid'],
    ],
  },
  {
    file: 'core/conversions.h',
    primitives: 'core/LayoutPrimitives.h',
    type: 'DisplayType',
    signature: 'inline int toInt(const DisplayType &displayType)',
    cases: [
      ['None', 0],
      ['Flex', 1],
      ['Contents', 2],
      ['Grid', 3],
    ],
  },
  {
    file: 'components/scrollview/conversions.h',
    primitives: 'components/scrollview/primitives.h',
    type: 'ScrollViewSnapToAlignment',
    signature: 'inline std::string toString(const ScrollViewSnapToAlignment &value)',
    cases: [
      ['Start', 'start'],
      ['Center', 'center'],
      ['End', 'end'],
    ],
  },
  {
    file: 'components/scrollview/conversions.h',
    primitives: 'components/scrollview/primitives.h',
    type: 'ScrollViewIndicatorStyle',
    signature: 'inline std::string toString(const ScrollViewIndicatorStyle &value)',
    cases: [
      ['Default', 'default'],
      ['Black', 'black'],
      ['White', 'white'],
    ],
  },
  {
    file: 'components/scrollview/conversions.h',
    primitives: 'components/scrollview/primitives.h',
    type: 'ScrollViewKeyboardDismissMode',
    signature: 'inline std::string toString(const ScrollViewKeyboardDismissMode &value)',
    cases: [
      ['None', 'none'],
      ['OnDrag', 'on-drag'],
      ['Interactive', 'interactive'],
    ],
  },
  {
    file: 'components/scrollview/conversions.h',
    primitives: 'components/scrollview/primitives.h',
    type: 'ContentInsetAdjustmentBehavior',
    signature: 'inline std::string toString(const ContentInsetAdjustmentBehavior &value)',
    cases: [
      ['Never', 'never'],
      ['Automatic', 'automatic'],
      ['ScrollableAxes', 'scrollableAxes'],
      ['Always', 'always'],
    ],
  },
  {
    file: 'components/image/ImageProps.cpp',
    primitives: 'imagemanager/primitives.h',
    type: 'ImageResizeMode',
    signature: 'inline std::string toString(ImageResizeMode resizeMode)',
    cases: [
      ['Cover', 'cover'],
      ['Contain', 'contain'],
      ['Stretch', 'stretch'],
      ['Center', 'center'],
      ['Repeat', 'repeat'],
      ['None', 'none'],
    ],
  },
  {
    file: 'mounting/ShadowViewMutation.cpp',
    primitives: 'mounting/ShadowViewMutation.h',
    type: 'ShadowViewMutation::Type',
    signature: 'std::string getDebugName(const ShadowViewMutation& mutation)',
    mutation: true,
    cases: [
      ['Create', 'Create'],
      ['Delete', 'Delete'],
      ['Insert', 'Insert'],
      ['Remove', 'Remove'],
      ['Update', 'Update'],
    ],
  },
  {
    file: 'mounting/ShadowTree.cpp',
    primitives: 'mounting/ShadowTree.h',
    type: 'ShadowTreeCommitSource',
    signature: 'std::string getShadowTreeCommitSourceName(ShadowTreeCommitSource source)',
    cases: [
      ['Unknown', 'Unknown'],
      ['React', 'React'],
      ['AnimationEndSync', 'AnimationEndSync'],
      ['ReactRevisionMerge', 'ReactRevisionMerge'],
    ],
  },
];

/** @param {string} source @param {string} marker */
function block(source, marker) {
  const start = source.indexOf(marker);
  assert.notEqual(start, -1, 'Missing pinned React Native declaration: ' + marker);
  const open = source.indexOf('{', start);
  let depth = 0;
  for (let index = open; index < source.length; index++) {
    if (source[index] === '{') depth++;
    if (source[index] === '}' && --depth === 0) return source.slice(start, index + 1);
  }
  throw new Error('Unterminated pinned declaration: ' + marker);
}

const declarations = new Map();
const functions = [];
const checks = [];
const invalid = [];
for (const [index, spec] of conversions.entries()) {
  const source = await readFile(new URL(spec.file, renderer), 'utf8');
  assert.ok(source.includes('\n#include <cstdlib>\n'), 'Missing abort declaration: ' + spec.file);
  const fn = block(source, spec.signature);
  assert.match(fn, /\n  std::abort\(\);\n\}$/);
  functions.push(fn);
  const primitive = await readFile(new URL(spec.primitives, renderer), 'utf8');
  const checked = spec.type === 'AccessibilityState::CheckedState';
  declarations.set(
    spec.type,
    spec.mutation
      ? 'struct ShadowViewMutation { ' +
          block(primitive, 'enum Type : std::uint8_t') +
          '; Type type; };'
      : checked
        ? 'struct AccessibilityState { ' + block(primitive, 'enum CheckedState') + '; };'
        : block(primitive, 'enum class ' + spec.type) + ';'
  );
  const method = spec.signature.match(/(\w+)\(/)?.[1];
  assert.ok(method, 'Missing pinned conversion function name: ' + spec.signature);
  /** @param {string} value */
  const call = (value) =>
    method + '(' + (spec.mutation ? 'ShadowViewMutation{' + value + '}' : value) + ')';
  for (const [tag, value] of spec.cases) {
    checks.push(
      'if (' + call(spec.type + '::' + tag) + ' != ' + JSON.stringify(value) + ') return 1;'
    );
  }
  // Only fixed-underlying enums admit arbitrary underlying values without UB.
  if (!checked)
    invalid.push({ name: String(index), call: call('static_cast<' + spec.type + '>(42)') + ';' });
}
for (const [file, signature] of [
  ['attributedstring/AttributedStringBox.cpp', 'bool operator==('],
  ['core/RawProps.cpp', 'folly::dynamic RawProps::toDynamic('],
]) {
  const source = await readFile(new URL(file, renderer), 'utf8');
  assert.ok(source.includes('\n#include <cstdlib>\n'), 'Missing abort declaration: ' + file);
  assert.match(block(source, signature), /\n  std::abort\(\);\n\}$/);
}
const animation = await readFile(
  new URL('animationbackend/AnimationBackend.cpp', renderer),
  'utf8'
);
const flushAssertion = block(
  animation,
  'void AnimationBackend::requestAsyncFlushForSurfaces('
).match(/react_native_assert\([\s\S]*?\);/)?.[0];
assert.ok(flushAssertion, 'Missing pinned async-flush invoker assertion.');
functions.push(
  'void checkAsyncFlushAssertion(bool hasInvoker, bool empty) {\n' +
    'int invoker{};\nauto* jsInvoker_ = hasInvoker ? &invoker : nullptr;\n' +
    'const std::set<int> surfaces = empty ? std::set<int>{} : std::set<int>{1};\n' +
    flushAssertion +
    '\n}'
);
const assertionChecks = [
  'checkAsyncFlushAssertion(true, true);',
  'checkAsyncFlushAssertion(true, false);',
  'checkAsyncFlushAssertion(false, true);',
];
const abortCases = [
  ...invalid,
  { name: 'async-flush-missing-invoker', call: 'checkAsyncFlushAssertion(false, false);' },
];
const compiler = spawnSync('c++', ['--version'], { encoding: 'utf8', timeout: 15000 });
const missingCompiler =
  compiler.error && 'code' in compiler.error && compiler.error.code === 'ENOENT';
if (process.platform !== 'linux' || missingCompiler) {
  console.log('Installed enum fallbacks verified; host C++ execution not run on this host.');
} else {
  assert.equal(compiler.status, 0, compiler.stderr);
  const root = await mkdtemp(path.join(os.tmpdir(), 'rnqnx-enum-conversions-'));
  try {
    const source = path.join(root, 'conversions.cpp');
    const executable = path.join(root, 'conversions');
    await writeFile(
      source,
      '#include <cstdint>\n#include <cstdlib>\n#include <set>\n#include <string>\n' +
        '#define react_native_assert(cond) do { if (!(cond)) std::abort(); } while (false)\n' +
        'namespace facebook::react {\n' +
        [...declarations.values(), ...functions].join('\n') +
        '\n}\nint main(int argc, char** argv) {\nusing namespace facebook::react;\nif (argc == 2) {\n' +
        abortCases
          .map((item) => 'if (std::string(argv[1]) == "' + item.name + '") ' + item.call)
          .join('\n') +
        '\nreturn 99;\n}\n' +
        checks.join('\n') +
        '\n' +
        assertionChecks.join('\n') +
        '\nreturn 0;\n}\n'
    );
    const build = spawnSync(
      'c++',
      ['-std=c++20', '-O0', '-Wall', '-Wextra', '-Werror', source, '-o', executable],
      { encoding: 'utf8', timeout: 30000 }
    );
    assert.equal(build.status, 0, build.stderr + String(build.error ?? ''));
    /** @param {string[]} args */
    const run = (args) =>
      spawnSync('sh', ['-c', 'ulimit -c 0; exec "$@"', 'sh', executable, ...args], {
        encoding: 'utf8',
        timeout: 15000,
      });
    assert.equal(run([]).status, 0, 'Valid enum conversions must retain their results.');
    for (const item of abortCases)
      assert.equal(
        run([item.name]).signal,
        'SIGABRT',
        'Invalid conversion/assertion must abort: ' + item.name
      );
    console.log(
      'React Native enum conversions: ' +
        checks.length +
        ' valid values and ' +
        invalid.length +
        ' invalid-value abort paths; async-flush assertion: 3 valid cases and 1 abort passed ' +
        '(host snippets only; no device claim).'
    );
  } finally {
    await rm(root, { recursive: true, force: true });
  }
}
