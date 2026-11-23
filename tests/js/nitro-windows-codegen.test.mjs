// @ts-nocheck: loads CommonJS modules through a Module._load hook and fakes the native class.
// Parser and renderer tests for the general spec path of the Nitro platform
// adapter (the QnxWindows spec): type table, rejected shapes, drift guards.
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import {
  generate,
  parseHybridSpec,
  renderGeneralBinding,
} from '../../scripts/nitro-platform-codegen.mjs';

const source = await readFile('packages/qnx-nitro-runtime/specs/QnxWindows.nitro.ts', 'utf8');
const spec = parseHybridSpec(source, 'QnxWindows');

assert.deepEqual(
  spec.structs.map((s) => s.name),
  ['Rect', 'TitleBarInsetData', 'WindowStateData']
);
assert.deepEqual(
  spec.props.map((p) => p.name),
  ['capabilities']
);
assert.deepEqual(
  spec.methods.map((m) => m.name),
  [
    'getState',
    'setControlsLayout',
    'setTitle',
    'setBounds',
    'minimize',
    'close',
    'addCloseGuard',
    'resolveCloseGuard',
    'addOnFocusChangedListener',
    'addOnVisibilityChangedListener',
    'getTitleBarInset',
    'addOnInsetChangedListener',
    'removeListener',
  ]
);

const output = renderGeneralBinding(spec);
assert.match(output, /struct JSIConverter<rnqnx::nitro::Rect> final/);
assert.match(output, /registerHybridProperty\("capabilities", &QnxWindows::getCapabilities\)/);
assert.match(output, /registerHybridMethod\("addCloseGuard", &QnxWindows::addCloseGuard\)/);
assert.match(output, /margelo::nitro::Callback<void\(double, double, std::string, bool, double\)>/);
assert.match(output, /margelo::nitro::Promise<rnqnx::nitro::Rect>|margelo::nitro::Promise<Rect>/);
// Drift guards: a signature that no longer matches the spec fails to compile.
assert.match(
  output,
  /static_assert\(std::is_same_v<MemberSignature<decltype\(&QnxWindows::setBounds\)>::Return/
);
assert.doesNotMatch(output, /Marathon/);

/** @type {Array<[string, string]>} */
const invalid = [
  ['unknown type', source.replace('setTitle(title: string)', 'setTitle(title: Date)')],
  [
    'number[] array',
    source.replace('readonly capabilities: string[]', 'readonly capabilities: number[]'),
  ],
  [
    'union type',
    source.replace(
      'layout: string): Promise<void>;\n  setTitle',
      'layout: "a" | "b"): Promise<void>;\n  setTitle'
    ),
  ],
  ['optional parameter', source.replace('close(force: boolean)', 'close(force?: boolean)')],
  ['optional struct field', source.replace('title: string;', 'title?: string;')],
  ['struct field of struct type', source.replace('title: string;', 'title: Rect;')],
  [
    'callback returning a value',
    source.replace('deadlineMs: number\n    ) => void', 'deadlineMs: number\n    ) => number'),
  ],
  [
    'callback as return type',
    source.replace('minimize(): Promise<void>', 'minimize(): () => void'),
  ],
  [
    'Promise as parameter',
    source.replace('close(force: boolean)', 'close(force: Promise<boolean>)'),
  ],
  ['void parameter', source.replace('close(force: boolean)', 'close(force: void)')],
  ['writable property', source.replace('readonly capabilities', 'capabilities')],
  ['static method', source.replace('getState(', 'static getState(')],
  ['generic class', source.replace('class QnxWindows ', 'class QnxWindows<T> ')],
  [
    'method body',
    source.replace(
      'minimize(): Promise<void>;',
      'minimize(): Promise<void> { return Promise.resolve(); }'
    ),
  ],
  [
    'duplicate member',
    source.replace(
      'minimize(): Promise<void>;',
      'minimize(): Promise<void>;\n  minimize(): Promise<void>;'
    ),
  ],
  ['top-level function', source + '\nexport function helper(): void {}\n'],
  ['second class', source + '\nexport declare class Other extends HybridObject {}\n'],
  ['missing return type', source.replace('minimize(): Promise<void>', 'minimize()')],
];
for (const [name, bad] of invalid) {
  assert.notEqual(bad, source, `${name} fixture must change the spec`);
  assert.throws(() => parseHybridSpec(bad, 'QnxWindows'), `${name} must be rejected`);
}
assert.throws(() => parseHybridSpec(source, 'OtherName'), 'class name must match the target');

await generate('--check');
console.log('nitro-windows-codegen.test.mjs passed');
