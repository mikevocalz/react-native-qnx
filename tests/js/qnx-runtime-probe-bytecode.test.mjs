import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { createRequire } from 'node:module';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { runInNewContext } from 'node:vm';

const root = path.resolve(import.meta.dirname, '../..');
const source = readFileSync(path.join(root, 'vnext/probes/runtime-qnx/main.cpp'), 'utf8');
const script = readFileSync(path.join(root, 'scripts/validate-runtime-qnx.sh'), 'utf8');
const loader = source.match(/std::string readBytecodeFile\(const char\* file\) \{[\s\S]*?^\}/m);
assert.ok(loader, 'exercise the actual probe binary-file reader');
assert.match(source, /argc != 3/);
assert.match(source, /IHermesRootAPI/);
assert.match(source, /isHermesBytecode\(buffer->data\(\), buffer->size\(\)\)/);
assert.match(source, /hermesBytecodeSanityCheck/);
assert.match(source, /std::move\(buffer\), label/);
assert.match(source, /catch \(const std::exception& error\)/);
assert.doesNotMatch(source, /StringBuffer>\(source\)|NitroModulesProxy\.getHybridClass/);
assert.match(script, /RNQNX_HOST_HERMESC:\[\^=\]\*=\/\/p/);
assert.match(script, /"\$\{HOST_HERMESC\}" -O -emit-binary/);
assert.match(script, /probe-bytecode-sha256\.txt/);
for (const name of ['nitro', 'expo']) {
  assert.ok(script.includes('"${EVIDENCE_DIR}/' + name + '.hbc"'));
  assert.ok(script.includes('"${QNX_TARGET_DIR}/runtime-' + name + '.hbc"'));
  assert.ok(script.includes('./runtime-' + name + '.hbc'));
}

const temporary = mkdtempSync(path.join(tmpdir(), 'rnqnx-probe-bytecode-'));
try {
  const cpp = path.join(temporary, 'reader.cpp');
  const executable = path.join(temporary, 'reader');
  writeFileSync(
    cpp,
    `#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string>
${loader[0]}
int main(int argc, char** argv) {
  if (argc != 2) return 2;
  try {
    const std::string bytes = readBytecodeFile(argv[1]);
    return std::fwrite(bytes.data(), 1, bytes.size(), stdout) == bytes.size() ? 0 : 3;
  } catch (const std::exception&) {
    return 17;
  }
}
`
  );
  const flags = ['-std=c++20', '-Wall', '-Wextra', '-Werror'];
  if (process.env.RNQNX_RUNTIME_PROBE_SANITIZERS === '1') {
    flags.push('-fsanitize=address,undefined', '-fno-sanitize-recover=all');
  }
  const compiled = spawnSync('c++', [...flags, cpp, '-o', executable], {
    encoding: 'utf8',
    timeout: 30000,
  });
  assert.equal(compiled.status, 0, compiled.stderr);
  /** @param {string} file */
  function read(file) {
    return spawnSync(executable, [file], { timeout: 5000, maxBuffer: 2 * 1024 * 1024 });
  }
  for (const bytes of [Buffer.from([0, 1, 255, 0, 3]), Buffer.alloc(1024 * 1024)]) {
    const file = path.join(temporary, 'binary');
    writeFileSync(file, bytes);
    const result = read(file);
    assert.equal(result.status, 0, result.stderr.toString());
    assert.deepEqual(result.stdout, bytes, 'binary data, including NUL, must not be truncated');
  }
  for (const bytes of [Buffer.alloc(0), Buffer.alloc(1024 * 1024 + 1)]) {
    const file = path.join(temporary, 'invalid');
    writeFileSync(file, bytes);
    assert.equal(read(file).status, 17, 'reject empty and oversized inputs');
  }
  assert.equal(read(path.join(temporary, 'missing')).status, 17);

  const require = createRequire(import.meta.url);
  const reactNativeRequire = createRequire(require.resolve('react-native/package.json'));
  const compilerPackage = reactNativeRequire.resolve('hermes-compiler/package.json');
  assert.equal(JSON.parse(readFileSync(compilerPackage, 'utf8')).version, '260318099.0.5');
  /** @type {Partial<Record<NodeJS.Platform, string>>} */
  const hosts = { linux: 'linux64-bin', darwin: 'osx-bin', win32: 'win64-bin' };
  const host = hosts[process.platform];
  assert.ok(host, 'use a supported pinned host compiler');
  const compiler = path.join(
    path.dirname(compilerPackage),
    'hermesc',
    host,
    process.platform === 'win32' ? 'hermesc.exe' : 'hermesc'
  );
  for (const name of ['nitro', 'expo']) {
    const fixture = path.join(root, 'vnext/probes/runtime-qnx', name + '.js');
    const output = path.join(temporary, name + '.hbc');
    const built = spawnSync(compiler, ['-O', '-emit-binary', '-out', output, fixture], {
      encoding: 'utf8',
      timeout: 15000,
    });
    assert.equal(built.status, 0, built.stderr);
    const bytecode = readFileSync(output);
    assert.equal(bytecode.subarray(0, 8).toString('hex'), 'c61fbc03c103191f');
    assert.deepEqual(read(output).stdout, bytecode, 'preserve actual compiler output bytes');
    const javascript = readFileSync(fixture, 'utf8');
    for (const architecture of ['aarch64', 'x86_64']) {
      /** @type {{platform: string, architecture: string, add: (a: number, b: number) => number}} */
      const platform = { platform: 'qnx', architecture, add: (a, b) => a + b };
      const result = runInNewContext(javascript, {
        NitroModulesProxy: {
          getHybridClass: (/** @type {string} */ className) => {
            assert.equal(className, 'QnxPlatformInfo');
            return class {
              constructor() {
                Object.assign(this, platform);
              }
            };
          },
        },
        expo: { modules: { ExpoQnxPlatform: platform } },
      });
      assert.equal(result, 'qnx|' + architecture + (name === 'nitro' ? '|42' : ''));
    }
    assert.throws(() => runInNewContext(javascript, {}), /not defined/);
  }
  console.log(
    'QNX runtime probe: binary-reader bounds, pinned HBC compilation and fixture source contracts passed (host only; no native API/device claim).'
  );
} finally {
  rmSync(temporary, { recursive: true, force: true });
}
