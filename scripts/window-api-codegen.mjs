#!/usr/bin/env node
// Constrained repository adapter for the audited Nitro v1 HybridClass ABI.
// This does not add an upstream Nitrogen QNX target.
import assert from 'node:assert/strict';
import { readFileSync, writeFileSync } from 'node:fs';
import { createRequire } from 'node:module';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
const require = createRequire(import.meta.url);
/** @type {typeof import('@typescript/typescript6')} */
const ts = require('@typescript/typescript6');
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const typePath = 'packages/qnx-nitro-runtime/specs/window/WindowSnapshot.ts';
const classPath = 'packages/qnx-nitro-runtime/specs/window/Window.nitro.ts';
const output = 'packages/qnx-nitro-runtime/cpp/generated/WindowBindings.hpp';
const publicOutput = 'packages/qnx-nitro-runtime/window.d.ts';
const expected = {
  WindowInsets: { top: 'number', right: 'number', bottom: 'number', left: 'number' },
  WindowSnapshot: {
    title: 'string',
    x: 'number',
    y: 'number',
    width: 'number',
    height: 'number',
    contentWidth: 'number',
    contentHeight: 'number',
    visible: 'boolean',
    focused: 'boolean',
    minimized: 'boolean',
    closeRequested: 'boolean',
    closeRequestId: 'string',
    controlsLayout: 'string',
    insets: 'WindowInsets',
    canSetPosition: 'boolean',
    canResize: 'boolean',
    canPreventClose: 'boolean',
    canMinimize: 'boolean',
  },
  WindowSubscription: { remove: '():void' },
  Window: {
    snapshot: 'WindowSnapshot',
    setTitle: '(title:string):Promise<void>',
    setSize: '(width:number,height:number):Promise<void>',
    setPosition: '(x:number,y:number):Promise<void>',
    setControlsLayout: '(layout:string):Promise<void>',
    minimize: '():Promise<void>',
    restore: '():Promise<void>',
    setVisible: '(visible:boolean):Promise<void>',
    requestFocus: '():Promise<void>',
    close: '():Promise<boolean>',
    cancelCloseRequest: '(requestId:string):Promise<boolean>',
    addSnapshotListener: '(listener:(snapshot:WindowSnapshot)=>void):WindowSubscription',
    addCloseGuard: '():WindowSubscription',
    dispose: '():void',
  },
  WindowHost: { getCurrentWindow: '():Window' },
};
/** @param {string} text */
const clean = (text) => text.replace(/\s+/g, '');
/** @typedef {{name: string, method: boolean, type: string, args: {name: string, type: string}[]}} Member */
/** @typedef {{name: string, cls: boolean, members: Member[]}} Declaration */
/** @param {string} types @param {string} classes @returns {Declaration[]} */
export function parseWindowSpec(types, classes) {
  /** @type {Record<string, Record<string, string>>} */
  const parsed = {};
  /** @type {Declaration[]} */
  const declarations = [];
  for (const [name, source] of [
    ['types.ts', types],
    ['Window.nitro.ts', classes],
  ]) {
    const file = ts.createSourceFile(name, source, ts.ScriptTarget.Latest, true);
    const diagnostics = 'parseDiagnostics' in file ? file.parseDiagnostics : [];
    assert.equal(
      Array.isArray(diagnostics) ? diagnostics.length : 0,
      0,
      'Invalid window spec syntax'
    );
    for (const node of file.statements) {
      if (ts.isImportDeclaration(node)) continue;
      assert.ok(
        ts.isInterfaceDeclaration(node) || ts.isClassDeclaration(node),
        'Unsupported declaration'
      );
      const cls = ts.isClassDeclaration(node);
      assert.ok(node.name && ts.isIdentifier(node.name), 'Named declaration required');
      assert.ok(!parsed[node.name.text], 'Duplicate declaration');
      assert.equal(node.typeParameters, undefined, 'Generics unsupported');
      assert.ok(
        node.modifiers?.some((m) => m.kind === ts.SyntaxKind.ExportKeyword),
        'Export required'
      );
      assert.ok(
        (node.modifiers ?? []).every(
          (m) =>
            m.kind === ts.SyntaxKind.ExportKeyword ||
            (cls && m.kind === ts.SyntaxKind.DeclareKeyword)
        ),
        'Unsupported modifiers'
      );
      if (cls) {
        assert.ok(
          node.modifiers?.some((m) => m.kind === ts.SyntaxKind.DeclareKeyword),
          'declare required'
        );
        assert.equal(node.heritageClauses?.length, 1, 'HybridObject base required');
        assert.ok(node.heritageClauses && node.heritageClauses.length === 1);
        assert.equal(node.heritageClauses[0].getText(file), 'extends HybridObject');
      } else assert.equal(node.heritageClauses, undefined, 'Interface extension unsupported');
      /** @type {Record<string, string>} */
      const fields = {};
      /** @type {Member[]} */
      const members = [];
      for (const member of node.members) {
        assert.ok(member.name && ts.isIdentifier(member.name), 'Only named members supported');
        assert.ok(!fields[member.name.text], 'Duplicate member');
        assert.ok(
          !('questionToken' in member) || member.questionToken === undefined,
          'Optional members unsupported'
        );
        const method = ts.isMethodDeclaration(member);
        assert.ok('type' in member && member.type, 'Explicit member type required');
        if (method) {
          assert.equal(member.modifiers, undefined, 'Method modifiers unsupported');
          assert.equal(member.typeParameters, undefined, 'Generic methods unsupported');
          assert.equal(member.body, undefined, 'Method bodies unsupported');
          assert.equal(member.asteriskToken, undefined, 'Generator methods unsupported');
          for (const p of member.parameters) {
            assert.ok(
              ts.isIdentifier(p.name) && p.name.text !== 'this',
              'Named value parameters required'
            );
            assert.equal(p.modifiers, undefined);
            assert.equal(p.questionToken, undefined);
            assert.equal(p.dotDotDotToken, undefined);
            assert.equal(p.initializer, undefined);
          }
          fields[member.name.text] = clean(
            `(${member.parameters.map((p) => p.getText(file)).join(',')}):${member.type.getText(file)}`
          );
        } else {
          assert.ok(
            ts.isPropertySignature(member) || ts.isPropertyDeclaration(member),
            'Unsupported member'
          );
          assert.equal(member.modifiers?.length, 1, 'Readonly fields required');
          assert.equal(member.modifiers[0].kind, ts.SyntaxKind.ReadonlyKeyword);
          assert.ok(!('initializer' in member) || member.initializer === undefined);
          assert.ok(!('exclamationToken' in member) || member.exclamationToken === undefined);
          fields[member.name.text] = clean(member.type.getText(file));
        }
        members.push({
          name: member.name.text,
          method,
          type: member.type.getText(file),
          args: method
            ? member.parameters.map((p) => {
                assert.ok(ts.isIdentifier(p.name) && p.type);
                return { name: p.name.text, type: p.type.getText(file) };
              })
            : [],
        });
      }
      parsed[node.name.text] = fields;
      declarations.push({ name: node.name.text, cls, members });
    }
  }
  assert.deepEqual(parsed, expected, 'Window API contract changed; review supported generation');
  return declarations;
}
/** @param {string} type @returns {string} */
function cppType(type) {
  const t = clean(type);
  if (t === 'string') return 'std::string';
  if (t === 'number') return 'double';
  if (t === 'boolean') return 'bool';
  if (t === 'void') return 'void';
  if (t.startsWith('Promise<')) return `margelo::nitro::Promise<${cppType(t.slice(8, -1))}>`;
  if (['Window', 'WindowSubscription', 'WindowHost'].includes(t))
    return `std::shared_ptr<${t}Spec>`;
  if (t === '(snapshot:WindowSnapshot)=>void')
    return 'margelo::nitro::Callback<void(WindowSnapshot)>';
  assert.ok(['WindowInsets', 'WindowSnapshot'].includes(t), 'Unsupported native type');
  return t;
}
/** @param {Declaration[]} declarations */
export function renderWindowBindings(declarations) {
  const lines = [
    '// Generated by scripts/window-api-codegen.mjs; do not edit.',
    '#pragma once',
    '#include <NitroModules/HybridObject.hpp>',
    '#include <NitroModules/HybridClass.hpp>',
    '#include <NitroModules/JSIConverter+NativeState.hpp>',
    '#include <NitroModules/JSIConverter+Callback.hpp>',
    '#include <NitroModules/JSIConverter+Promise.hpp>',
    '#include <stdexcept>',
    'namespace rnqnx::nitro::windowapi {',
  ];
  for (const d of declarations.filter((d) => !d.cls)) {
    lines.push(`struct ${d.name} {`);
    for (const m of d.members) lines.push(`  ${cppType(m.type)} ${m.name}{};`);
    lines.push('};');
  }
  lines.push('} // namespace rnqnx::nitro::windowapi', 'namespace margelo::nitro {');
  for (const d of declarations.filter((d) => !d.cls)) {
    const native = `rnqnx::nitro::windowapi::${d.name}`;
    lines.push(
      `template <> struct JSIConverter<${native}> final {`,
      '  static bool canConvert(facebook::jsi::Runtime& runtime, const facebook::jsi::Value& value) {',
      '    if (!value.isObject()) return false;',
      '    auto object = value.asObject(runtime);',
      `    return ${d.members.map((m) => `JSIConverter<${cppType(m.type).replace(/^Window/, 'rnqnx::nitro::windowapi::Window')}>::canConvert(runtime, object.getProperty(runtime, "${m.name}"))`).join(' &&\n        ')};`,
      '  }',
      `  static ${native} fromJSI(facebook::jsi::Runtime& runtime, const facebook::jsi::Value& value) {`,
      `    if (!canConvert(runtime, value)) throw std::invalid_argument("Expected ${d.name}");`,
      '    auto object = value.asObject(runtime);',
      `    return ${native}{`,
      ...d.members.map(
        (m) =>
          `      JSIConverter<${cppType(m.type).replace(/^Window/, 'rnqnx::nitro::windowapi::Window')}>::fromJSI(runtime, object.getProperty(runtime, "${m.name}")),`
      ),
      '    };',
      '  }',
      `  static facebook::jsi::Value toJSI(facebook::jsi::Runtime& runtime, const ${native}& value) {`,
      '    facebook::jsi::Object object(runtime);',
      ...d.members.map(
        (m) =>
          `    object.setProperty(runtime, "${m.name}", JSIConverter<${cppType(m.type).replace(/^Window/, 'rnqnx::nitro::windowapi::Window')}>::toJSI(runtime, value.${m.name}));`
      ),
      '    return object;',
      '  }',
      '};'
    );
  }
  lines.push('} // namespace margelo::nitro', 'namespace rnqnx::nitro::windowapi {');
  for (const d of declarations.filter((d) => d.cls)) lines.push(`class ${d.name}Spec;`);
  for (const d of declarations.filter((d) => d.cls)) {
    lines.push(
      `class ${d.name}Spec : public margelo::nitro::HybridObject {`,
      ' public:',
      '  struct Class {',
      '    using Base = margelo::nitro::HybridObject::Class;',
      `    static constexpr const char* name = "Qnx${d.name}";`,
      '    static const std::shared_ptr<const margelo::nitro::HybridClass>& get();',
      '  };'
    );
    for (const m of d.members)
      lines.push(
        `  virtual ${cppType(m.type)} ${m.method ? m.name : `get${m.name[0].toUpperCase()}${m.name.slice(1)}`}(${m.args.map((p) => `${cppType(p.type)} ${p.name}`).join(', ')})${m.name === 'dispose' ? ' override' : ''} = 0;`
      );
    lines.push(
      ' protected:',
      '  const std::shared_ptr<const margelo::nitro::HybridClass>& getHybridClass() override { return Class::get(); }',
      '};'
    );
  }
  for (const d of declarations.filter((d) => d.cls)) {
    lines.push(
      `inline const std::shared_ptr<const margelo::nitro::HybridClass>& ${d.name}Spec::Class::get() {`,
      '  static const auto descriptor = margelo::nitro::HybridClass::create<Class>([](margelo::nitro::HybridClass& value) {'
    );
    for (const m of d.members.filter((m) => m.name !== 'dispose'))
      lines.push(
        `    value.registerHybrid${m.method ? 'Method' : 'Property'}("${m.name}", &${d.name}Spec::${m.method ? m.name : `get${m.name[0].toUpperCase()}${m.name.slice(1)}`});`
      );
    lines.push('  });', '  return descriptor;', '}');
  }
  lines.push('} // namespace rnqnx::nitro::windowapi', '');
  return lines.join('\n');
}
/** @param {Declaration[]} declarations */
export function renderWindowDeclarations(declarations) {
  const lines = [
    '// Generated by scripts/window-api-codegen.mjs; do not edit.',
    "import type { WindowSnapshot } from './specs/window/WindowSnapshot';",
    "export type { WindowSnapshot, WindowInsets } from './specs/window/WindowSnapshot';",
  ];
  for (const d of declarations.filter((d) => d.cls && d.name !== 'WindowHost')) {
    lines.push(`export interface ${d.name} {`);
    for (const m of d.members)
      lines.push(
        m.method
          ? `  ${m.name}(${m.args.map((p) => `${p.name}: ${p.type}`).join(', ')}): ${m.type};`
          : `  readonly ${m.name}: ${m.type};`
      );
    lines.push('}');
  }
  lines.push('export function getCurrentWindow(): Window;', '');
  return lines.join('\n');
}
/** @param {'--check' | '--write'} [mode] */
export function verifyWindowBindings(mode = '--check') {
  const generated = renderWindowBindings(
    parseWindowSpec(
      readFileSync(path.join(root, typePath), 'utf8'),
      readFileSync(path.join(root, classPath), 'utf8')
    )
  );
  const publicTypes = renderWindowDeclarations(
    parseWindowSpec(
      readFileSync(path.join(root, typePath), 'utf8'),
      readFileSync(path.join(root, classPath), 'utf8')
    )
  );
  if (mode === '--write') {
    writeFileSync(path.join(root, output), generated);
    writeFileSync(path.join(root, publicOutput), publicTypes);
  } else {
    assert.equal(
      readFileSync(path.join(root, output), 'utf8'),
      generated,
      'Window bindings drift; run node scripts/window-api-codegen.mjs --write'
    );
    assert.equal(
      readFileSync(path.join(root, publicOutput), 'utf8'),
      publicTypes,
      'Window declarations drift'
    );
  }
  console.log('Window API constrained generation: OK');
}
if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  verifyWindowBindings(process.argv.includes('--write') ? '--write' : '--check');
}
