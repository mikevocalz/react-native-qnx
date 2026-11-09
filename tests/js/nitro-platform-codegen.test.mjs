import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { generate, parseSpec, renderBinding } from '../../scripts/nitro-platform-codegen.mjs';

const source = await readFile('packages/qnx-nitro-runtime/specs/PlatformInfo.nitro.ts', 'utf8');
const parsed = parseSpec(source);
assert.deepEqual(parsed, {
  props: ['platform', 'architecture'],
  methods: ['add'],
});

/** @type {Array<[string, string]>} */
const invalidSpecs = [
  ['non-numeric parameter', source.replace('left: number', 'left: string')],
  ['writable property', source.replace('readonly architecture', 'architecture')],
  [
    'unsupported method signature',
    source.replace('add(left: number, right: number): number', 'add(): void'),
  ],
  [
    'optional property',
    source.replace('readonly platform: string;', 'readonly platform?: string;'),
  ],
  ['static property', source.replace('readonly platform:', 'static readonly platform:')],
  ['private property', source.replace('readonly platform:', 'private readonly platform:')],
  ['protected property', source.replace('readonly platform:', 'protected readonly platform:')],
  [
    'property initializer',
    source.replace('readonly platform: string;', 'readonly platform: string = "qnx";'),
  ],
  [
    'definite-assignment property',
    source.replace('readonly platform: string;', 'readonly platform!: string;'),
  ],
  ['static method', source.replace('add(', 'static add(')],
  ['private method', source.replace('add(', 'private add(')],
  ['protected method', source.replace('add(', 'protected add(')],
  ['optional method', source.replace('add(', 'add?(')],
  ['generic method', source.replace('add(', 'add<T>(')],
  ['generator method', source.replace('add(', '*add(')],
  [
    'method body',
    source.replace(
      'add(left: number, right: number): number;',
      'add(left: number, right: number): number { return left + right; }'
    ),
  ],
  ['rest parameter', source.replace('right: number', '...right: number')],
  ['this parameter', source.replace('left: number', 'this: number')],
  ['destructured parameter', source.replace('left: number', '{ left }: number')],
  ['generic class', source.replace('class PlatformInfo ', 'class PlatformInfo<T> ')],
  ['abstract class', source.replace('declare class', 'declare abstract class')],
  ['implements-only heritage', source.replace('extends HybridObject', 'implements HybridObject')],
  [
    'additional heritage',
    source.replace('extends HybridObject', 'extends HybridObject implements Extra'),
  ],
  ['generic heritage', source.replace('extends HybridObject', 'extends HybridObject<string>')],
];
for (const [name, invalid] of invalidSpecs) {
  assert.notEqual(invalid, source, `${name} fixture must change the spec`);
  assert.throws(() => parseSpec(invalid), `${name} must not generate instance bindings`);
}
// Explicit public visibility preserves the same instance API.
assert.deepEqual(
  parseSpec(
    source.replace('readonly platform:', 'public readonly platform:').replace('add(', 'public add(')
  ),
  parsed
);
const qnx = renderBinding(parsed, {
  className: 'QnxPlatformInfo',
  output: 'ignored',
});
const marathon = renderBinding(parsed, {
  className: 'MarathonPlatformInfo',
  output: 'ignored',
});
assert.match(qnx, /QnxPlatformInfo::Class::get/);
assert.match(marathon, /MarathonPlatformInfo::Class::get/);
assert.match(qnx, /registerHybridMethod\("add"/);
assert.match(marathon, /registerHybridProperty\("architecture"/);
assert.doesNotMatch(marathon, /QnxPlatformInfo/);
await generate('--check');
console.log('nitro-platform-codegen.test.mjs passed (generated bindings only)');
