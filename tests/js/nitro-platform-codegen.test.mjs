import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { generate, parseSpec, renderBinding } from '../../scripts/nitro-platform-codegen.mjs';

const source = await readFile('packages/qnx-nitro-runtime/specs/PlatformInfo.nitro.ts', 'utf8');
const parsed = parseSpec(source);
assert.deepEqual(parsed, {
  props: ['platform', 'architecture'],
  methods: ['add'],
});

for (const invalid of [
  source.replace('left: number', 'left: string'),
  source.replace('readonly architecture', 'architecture'),
  source.replace('add(left: number, right: number): number', 'add(): void'),
  source.replace('readonly platform: string;', 'readonly platform?: string;'),
]) {
  assert.throws(() => parseSpec(invalid));
}
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
