#!/usr/bin/env node
// Generates the per-function native API reference from the public C++ headers and the
// Nitro specs. Output goes to content/docs/reference/native-api/.
//   node scripts/generate-api.mjs          write files
//   node scripts/generate-api.mjs --check  fail if committed files differ
//
// Inputs are the tracked *.h and *.hpp files (git ls-files) and the tracked *.nitro.ts files.
// Nothing here reads the network, a timestamp, an absolute path or a commit SHA, so two runs
// over the same tree are byte-identical.
//
// The C++ scanner is a small, tolerant, brace-depth scanner. It is not a C++ parser. Anything
// it cannot classify is counted and listed on the index page ("Not parsed") instead of dropped.
import { execFileSync } from 'node:child_process';
import {
  readFileSync,
  writeFileSync,
  readdirSync,
  existsSync,
  mkdirSync,
  unlinkSync,
} from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const site = resolve(here, '..');
const repo = resolve(site, '..');
const check = process.argv.includes('--check');
const BLOB = 'https://github.com/mikevocalz/react-native-qnx/blob/main/';
const OUT_DIR = 'content/docs/reference/native-api';
const DOCS_URL = '/docs/reference/native-api';
const banned = new RegExp(['mara', 'thon'].join(''), 'i');
const NO_DOC = 'No doc comment in the header.';
const BANNED_DOC_NOTICE =
  'A doc comment exists in the source. It is omitted here because it contains a term the docs build bans.';

const git = (...args) =>
  execFileSync('git', args, { cwd: repo, encoding: 'utf8', maxBuffer: 1 << 28 });
const cmp = (a, b) => (a < b ? -1 : a > b ? 1 : 0);
const read = (rel) => readFileSync(join(repo, rel), 'utf8');
const esc = (s) => s.replace(/\|/g, '\\|').replace(/[<>{}]/g, (c) => `\\${c}`);
const plainDash = (s) => s.replace(/[ \t]*[\u2013\u2014][ \t]*/g, ' - ');
const srcLink = (path, line) => `[${path}#L${line}](${BLOB}${path}#L${line})`;
const fence = (text, lang) => {
  const longest = Math.max(2, ...[...text.matchAll(/`+/g)].map((m) => m[0].length));
  const f = '`'.repeat(longest + 1);
  return `${f}${lang}\n${text}\n${f}`;
};

// ---------------------------------------------------------------------------------------------
// Lexer: blanks comments, preprocessor lines and the punctuation inside string literals so the
// scanner can count braces safely, and collects comments and directives with line numbers.
// ---------------------------------------------------------------------------------------------
function lex(S, mode) {
  const n = S.length;
  const M = Array.from(S);
  const P = Array.from(S);
  const lineStarts = [0];
  for (let i = 0; i < n; i++) if (S[i] === '\n') lineStarts.push(i + 1);
  const lineOf = (idx) => {
    let lo = 0;
    let hi = lineStarts.length - 1;
    while (lo < hi) {
      const mid = (lo + hi + 1) >> 1;
      if (lineStarts[mid] <= idx) lo = mid;
      else hi = mid - 1;
    }
    return lo + 1;
  };
  const onlyWsBefore = (idx) => /^[ \t]*$/.test(S.slice(lineStarts[lineOf(idx) - 1], idx));
  const blank = (arr, a, b) => {
    for (let k = a; k < b; k++) if (S[k] !== '\n' && S[k] !== '\r') arr[k] = ' ';
  };
  const comments = [];
  const directives = [];
  const maskLiteral = (a, b) => {
    for (let k = a + 1; k < b - 1; k++) if (/[{}();<>,=[\]]/.test(S[k])) M[k] = ' ';
  };
  let i = 0;
  while (i < n) {
    const c = S[i];
    const d = S[i + 1];
    if (c === '/' && d === '/') {
      let j = S.indexOf('\n', i);
      if (j < 0) j = n;
      comments.push({
        s: i,
        e: j,
        sl: lineOf(i),
        el: lineOf(i),
        text: S.slice(i, j),
        block: false,
        alone: onlyWsBefore(i),
      });
      blank(M, i, j);
      blank(P, i, j);
      i = j;
      continue;
    }
    if (c === '/' && d === '*') {
      let j = S.indexOf('*/', i + 2);
      j = j < 0 ? n : j + 2;
      comments.push({
        s: i,
        e: j,
        sl: lineOf(i),
        el: lineOf(j - 1),
        text: S.slice(i, j),
        block: true,
        alone: onlyWsBefore(i),
      });
      blank(M, i, j);
      blank(P, i, j);
      i = j;
      continue;
    }
    if (mode === 'cpp' && c === '#' && onlyWsBefore(i)) {
      let j = i;
      for (;;) {
        const k = S.indexOf('\n', j);
        if (k < 0) {
          j = n;
          break;
        }
        if (S[k - 1] === '\\' || (S[k - 1] === '\r' && S[k - 2] === '\\')) {
          j = k + 1;
          continue;
        }
        j = k;
        break;
      }
      directives.push({ line: lineOf(i), text: S.slice(i, j) });
      blank(M, i, j);
      blank(P, i, j);
      i = j;
      continue;
    }
    if (mode === 'cpp' && c === '"' && S[i - 1] === 'R') {
      const open = S.indexOf('(', i);
      const delim = open > 0 ? S.slice(i + 1, open) : '';
      const close = S.indexOf(`)${delim}"`, open);
      const j = close < 0 ? n : close + delim.length + 2;
      for (let k = i + 1; k < j - 1; k++) if (S[k] !== '\n') M[k] = ' ';
      i = j;
      continue;
    }
    if (c === '"' || (mode === 'ts' && (c === "'" || c === '`'))) {
      let j = i + 1;
      while (j < n && S[j] !== c) {
        if (S[j] === '\\') j++;
        else if (S[j] === '\n' && c !== '`') break;
        j++;
      }
      j = Math.min(n, j + 1);
      maskLiteral(i, j);
      i = j;
      continue;
    }
    if (mode === 'cpp' && c === "'") {
      const prev = S[i - 1] ?? '';
      const next = S[i + 1] ?? '';
      if (/[0-9a-fA-F]/.test(prev) && /[0-9a-fA-F]/.test(next) && S[i + 2] !== "'") {
        i++;
        continue;
      }
      let j = i + 1;
      while (j < n && S[j] !== "'" && S[j] !== '\n') {
        if (S[j] === '\\') j++;
        j++;
      }
      j = Math.min(n, j + 1);
      maskLiteral(i, j);
      i = j;
      continue;
    }
    i++;
  }
  return { M: M.join(''), P: P.join(''), lineOf, lineStarts, comments, directives };
}

const matching = (M, open, o, c) => {
  let d = 0;
  for (let k = open; k < M.length; k++) {
    if (M[k] === o) d++;
    else if (M[k] === c && --d === 0) return k;
  }
  return -1;
};

// Doc comment: the run of standalone comment lines that ends on the line right above `line`.
function docFor(lx, line) {
  const byEnd = new Map();
  for (const c of lx.comments) if (c.alone) byEnd.set(c.el, c);
  const run = [];
  let cur = line - 1;
  while (byEnd.has(cur)) {
    const c = byEnd.get(cur);
    run.unshift(c);
    cur = c.sl - 1;
  }
  if (!run.length) return null;
  const lines = [];
  for (const c of run) {
    if (!c.block) {
      lines.push(c.text.replace(/^\/\/[/!]?<? ?/, '').replace(/\s+$/, ''));
    } else {
      const inner = c.text.replace(/^\/\*+[!<]?/, '').replace(/\*+\/$/, '');
      inner.split('\n').forEach((l, idx) => {
        const t = idx === 0 ? l.replace(/^ /, '') : l.replace(/^\s*\* ?/, '');
        lines.push(t.replace(/\s+$/, ''));
      });
    }
  }
  while (lines.length && !lines[0].trim()) lines.shift();
  while (lines.length && !lines[lines.length - 1].trim()) lines.pop();
  return lines.length ? plainDash(lines.join('\n')) : null;
}

// Print form of a statement: comments already blanked in P, continuation lines dedented.
function printSig(lx, a, b) {
  const text = lx.P.slice(a, b);
  const col = a - lx.lineStarts[lx.lineOf(a) - 1];
  const out = text
    .split('\n')
    .map((l, idx) =>
      (idx === 0 ? l : l.replace(new RegExp(`^ {0,${col}}`), '')).replace(/\s+$/, '')
    )
    .filter((l) => l.trim());
  return plainDash(out.join('\n').trim());
}

// ---------------------------------------------------------------------------------------------
// C++ scanner
// ---------------------------------------------------------------------------------------------
const BLANK_BARE =
  /\b(Q_OBJECT|Q_GADGET|Q_NAMESPACE|QML_ELEMENT|QML_ANONYMOUS|QML_SINGLETON|Q_SIGNAL|Q_SLOT|QT_BEGIN_NAMESPACE|QT_END_NAMESPACE|QT_BEGIN_MOC_NAMESPACE|QT_END_MOC_NAMESPACE)\b/g;
const BLANK_CALL =
  /\b(Q_ENUM|Q_FLAG|Q_ENUM_NS|Q_FLAG_NS|Q_DECLARE_FLAGS|Q_DECLARE_METATYPE|Q_DISABLE_COPY|Q_DISABLE_COPY_MOVE|Q_DECLARE_OPERATORS_FOR_FLAGS|QML_NAMED_ELEMENT|QML_UNCREATABLE|Q_CLASSINFO|Q_INTERFACES|Q_PRIVATE_SLOT|Q_DECLARE_INTERFACE|Q_DECLARE_PRIVATE|Q_DECLARE_PUBLIC|Q_REVISION|QML_ADDED_IN_VERSION|QML_EXTENDED|QML_FOREIGN|QML_VALUE_TYPE)\s*\(/g;

function neutralizeQt(M) {
  const arr = Array.from(M);
  const blankRange = (a, b) => {
    for (let k = a; k < b; k++) if (arr[k] !== '\n') arr[k] = ' ';
  };
  for (const m of M.matchAll(BLANK_BARE)) blankRange(m.index, m.index + m[0].length);
  for (const m of M.matchAll(BLANK_CALL)) {
    const open = m.index + m[0].length - 1;
    const close = matching(M, open, '(', ')');
    if (close > 0) blankRange(m.index, close + 1);
  }
  for (const m of M.matchAll(/\bQ_PROPERTY\s*\(/g)) {
    const open = m.index + m[0].length - 1;
    const close = matching(M, open, '(', ')');
    if (close < 0) continue;
    for (let k = open; k < close; k++) if (arr[k] === '(' || arr[k] === ')') arr[k] = ' ';
    arr[open] = ' ';
    arr[close] = ';';
  }
  return arr.join('');
}

// Replaces every balanced {...} group with {} so initializer expressions cannot look like calls.
const stripBraces = (t) => {
  let out = '';
  let d = 0;
  for (const c of t) {
    if (c === '{') {
      if (d++ === 0) out += '{}';
    } else if (c === '}') d = Math.max(0, d - 1);
    else if (d === 0) out += c;
  }
  return out;
};

const stripLeadingTemplate = (t) => {
  if (!/^template\s*</.test(t)) return t;
  let d = 0;
  for (let k = t.indexOf('<'); k < t.length; k++) {
    if (t[k] === '<') d++;
    else if (t[k] === '>' && --d === 0) return t.slice(k + 1).trim();
  }
  return t;
};

// Index of the first '(' outside angle brackets, or -1. Handles operator() and operator<.
function findFuncParen(t) {
  let angle = 0;
  for (let k = 0; k < t.length; k++) {
    const c = t[k];
    if (c === 'o' && /^operator\b/.test(t.slice(k)) && (k === 0 || !/\w/.test(t[k - 1]))) {
      let q = k + 8;
      while (t[q] === ' ') q++;
      if (t[q] === '(' && t[q + 1] === ')') q += 2;
      else {
        while (q < t.length && t[q] !== '(') q++;
        return q < t.length ? q : -1;
      }
      while (t[q] === ' ') q++;
      return t[q] === '(' ? q : -1;
    }
    if (c === '<' && (/[\w>]/.test(t[k - 1] ?? '') || /\s/.test(t[k - 1] ?? ''))) angle++;
    else if (c === '>' && t[k - 1] !== '-' && angle > 0) angle--;
    else if (c === '=' && angle === 0 && t[k + 1] !== '=') return -1;
    else if (c === '(' && angle === 0) return k;
  }
  return -1;
}

const lastIdent = (t) => {
  const m = t.match(/([A-Za-z_]\w*)\s*$/);
  return m ? m[1] : '';
};

function scanCpp(file, S) {
  const lx = lex(S, 'cpp');
  const M = neutralizeQt(lx.M);
  const n = M.length;
  const out = { decls: [], unparsed: [], convSkipped: 0, bannedSkipped: 0 };
  const stack = [{ kind: 'file', reach: true, conv: false, name: '' }];
  const top = () => stack[stack.length - 1];
  const nsChain = () =>
    stack
      .filter((s) => s.kind === 'namespace' && s.name)
      .map((s) => s.name)
      .join('::');
  const clsChain = () =>
    stack
      .filter((s) => s.kind === 'class' && s.name)
      .map((s) => s.name)
      .join('::');
  const unparsed = (idx, reason) => out.unparsed.push({ file, line: lx.lineOf(idx), reason });

  const record = (kind, name, start, endExclusive, extra = {}) => {
    const sc = top();
    // Q_PROPERTY is visible to QML whatever the C++ access section says.
    if (sc.kind === 'class' && !sc.pub && kind !== 'property') return;
    if (!sc.reach) {
      if (sc.conv) out.convSkipped++;
      return;
    }
    const line = lx.lineOf(start);
    const sig = printSig(lx, start, endExclusive);
    const doc = docFor(lx, line);
    if (banned.test(sig) || banned.test(name)) {
      out.bannedSkipped++;
      return;
    }
    out.decls.push({
      file,
      line,
      kind,
      name,
      ns: nsChain(),
      cls: clsChain(),
      sig,
      doc: doc && banned.test(doc) ? { omitted: true } : doc,
      ...extra,
    });
  };

  const sectionKind = (sc) =>
    sc.sect === 'signals' ? 'signal' : sc.sect === 'slots' ? 'slot' : null;

  // Directives: public macros.
  const guard = new Set();
  for (const d of lx.directives) {
    const g = d.text.match(/^#\s*ifndef\s+(\w+)/);
    if (g) guard.add(g[1]);
  }
  for (const d of lx.directives) {
    const m = d.text.match(/^#\s*define\s+([A-Za-z_]\w*)(\([^)]*\))?/);
    if (!m || guard.has(m[1])) continue;
    const text = plainDash(d.text.replace(/\\\r?\n/g, '\\\n').replace(/\s+$/, ''));
    if (banned.test(text)) {
      out.bannedSkipped++;
      continue;
    }
    const doc = docFor(lx, d.line);
    out.decls.push({
      file,
      line: d.line,
      kind: 'macro',
      name: m[1],
      ns: '',
      cls: '',
      sig: text.split('\n').slice(0, 12).join('\n'),
      doc: doc && banned.test(doc) ? { omitted: true } : doc,
    });
  }

  const classify = (start, end, term) => {
    const raw = M.slice(start, end);
    let t = stripBraces(raw).replace(/\s+/g, ' ').trim();
    if (!t) return;
    const sc = top();
    if (/^using\s+namespace\b/.test(t) || /^static_assert\b/.test(t) || /^friend\b/.test(t)) return;
    if (/=\s*delete\s*$/.test(t)) return;
    if (/^(extern\s+)?template\s+(class|struct)\b/.test(t)) return;
    const core = stripLeadingTemplate(t).replace(/^(\[\[[^\]]*\]\]\s*)+/, '');
    if (/^(class|struct|union)\b[^=()]*$/.test(core) && !/[{]/.test(core)) {
      if (/^(class|struct|union)\s+(\w+\s*)?(\w+\s*)?$/.test(core)) return; // forward declaration
    }
    if (
      /^enum\b[^=()]*$/.test(core) &&
      /^enum(\s+(class|struct))?\s+\w+(\s*:\s*[\w:\s]+)?$/.test(core)
    )
      return;
    if (/^Q_PROPERTY\b/.test(core)) {
      const m = raw.match(
        /Q_PROPERTY\s+.*?(\w+)\s+(READ|MEMBER|WRITE|NOTIFY|RESET|REVISION|DESIGNABLE|SCRIPTABLE|STORED|USER|CONSTANT|FINAL|REQUIRED|BINDABLE)\b/
      );
      if (!m) return unparsed(start, 'Q_PROPERTY without a recognizable name');
      return record('property', m[1], start, end + 1);
    }
    if (/^(using|typedef)\b/.test(core)) {
      let name = '';
      const u = core.match(/^using\s+([A-Za-z_]\w*)\s*=/);
      if (u) name = u[1];
      else if (/^using\b/.test(core)) name = lastIdent(core.replace(/;?\s*$/, ''));
      else {
        const fp = core.match(/\(\s*\*\s*([A-Za-z_]\w*)\s*\)/);
        name = fp ? fp[1] : lastIdent(core.replace(/\[[^\]]*\]\s*$/, ''));
      }
      if (!name) return unparsed(start, 'alias without a name');
      return record('alias', name, start, end + (term === ';' ? 1 : 0));
    }
    const noSpec = core
      .replace(
        /^((Q_INVOKABLE|static|virtual|inline|constexpr|consteval|constinit|explicit|extern|mutable|thread_local|\[\[[^\]]*\]\])\s+)+/,
        ''
      )
      .trim();
    const p = findFuncParen(noSpec);
    const inClass = sc.kind === 'class';
    if (p > 0 && !/^[(*&]/.test(noSpec.slice(p + 1).trim())) {
      const pre = noSpec.slice(0, p).trim();
      let name;
      if (/\boperator\b/.test(pre))
        name = pre.slice(pre.search(/\boperator\b/)).replace(/\s+/g, ' ');
      else {
        const m = pre.match(/((?:[A-Za-z_]\w*(?:<[^()]*>)?\s*::\s*)*)(~?[A-Za-z_]\w*)\s*$/);
        name = m ? m[2] : '';
      }
      if (!name) return unparsed(start, 'function without a recognizable name');
      let kind = inClass ? 'method' : 'function';
      if (inClass) {
        if (name.startsWith('~')) kind = 'destructor';
        else if (name === sc.name) kind = 'constructor';
        else if (sectionKind(sc)) kind = sectionKind(sc);
      }
      return record(kind, name, start, end);
    }
    // function pointer, variable, field, constant
    let name = '';
    const fp = noSpec.match(/\(\s*[*&]\s*([A-Za-z_]\w*)\s*\)\s*\(/);
    if (fp) name = fp[1];
    else {
      const head = noSpec.split(/=|\{|\[|(?<!:):(?!:)/)[0];
      name = lastIdent(head.replace(/[*&\s]+$/, ''));
    }
    if (!name) return unparsed(start, 'declaration without a recognizable name');
    return record(inClass ? 'field' : 'variable', name, start, end);
  };

  let i = 0;
  const accessRe =
    /\s*(public|private|protected)(\s+(Q_SLOTS|slots))?\s*:(?!:)|\s*(Q_SIGNALS|signals)\s*:(?!:)|\s*(Q_SLOTS|slots)\s*:(?!:)/y;
  while (i < n) {
    while (i < n && /\s/.test(M[i])) i++;
    if (i >= n) break;
    if (M[i] === ';') {
      i++;
      continue;
    }
    if (M[i] === '}') {
      const sc = stack.length > 1 ? stack.pop() : null;
      if (!sc) {
        unparsed(i, 'closing brace without an open scope');
        i++;
        continue;
      }
      i++;
      if (sc.kind === 'class') {
        const semi = M.indexOf(';', i);
        if (semi > 0 && /^[\s\w*&,[\]]*$/.test(M.slice(i, semi))) i = semi + 1;
      }
      continue;
    }
    accessRe.lastIndex = i;
    const am = accessRe.exec(M);
    if (am && top().kind === 'class') {
      const sc = top();
      if (am[1]) {
        sc.pub = am[1] === 'public';
        sc.sect = am[3] ? 'slots' : null;
      } else if (am[4]) {
        sc.pub = true;
        sc.sect = 'signals';
      } else {
        sc.pub = true;
        sc.sect = 'slots';
      }
      i = accessRe.lastIndex;
      continue;
    }
    const start = i;
    let j = i;
    let p = 0;
    let br = 0;
    let term = null;
    let initList = false;
    scan: for (; j < n; j++) {
      const c = M[j];
      if (c === '(') p++;
      else if (c === ')') p = Math.max(0, p - 1);
      else if (c === '[') br++;
      else if (c === ']') br = Math.max(0, br - 1);
      else if (p === 0 && br === 0) {
        if (c === ';') {
          term = ';';
          break scan;
        }
        if (c === '}') {
          term = '}';
          break scan;
        }
        if (c === '{') {
          const head = M.slice(start, j).replace(/\s+/g, ' ').trim();
          const core = stripLeadingTemplate(head).replace(/^(\[\[[^\]]*\]\]\s*)+/, '');
          // brace initializer inside a variable or an in-progress constructor initializer list
          if (initList && /[\w>]$/.test(head)) {
            const close = matching(M, j, '{', '}');
            if (close < 0) break scan;
            j = close;
            continue;
          }
          if (
            /^(inline\s+)?namespace\b/.test(core) ||
            /^extern\s+"[^"]*"$/.test(core) ||
            /^(class|struct|union)\b/.test(core)
          ) {
            term = '{scope';
            break scan;
          }
          if (/^enum\b/.test(core)) {
            term = '{enum';
            break scan;
          }
          const fp = findFuncParen(core);
          if (fp > 0) {
            const close = matching(core, fp, '(', ')');
            const rest = close > 0 ? core.slice(close + 1) : '';
            if (/(^|[^:]):(?!:)/.test(rest.replace(/\?[^:]*:/g, ''))) {
              initList = true;
              const close2 = matching(M, j, '{', '}');
              if (/[\w>]$/.test(head) && close2 > 0) {
                j = close2;
                continue;
              }
            }
            term = '{func';
            break scan;
          }
          if (/=\s*$/.test(head) || !/[(]/.test(head)) {
            const close = matching(M, j, '{', '}');
            if (close < 0) break scan;
            j = close;
            continue;
          }
          term = '{unknown';
          break scan;
        }
      }
    }
    if (term === null) {
      unparsed(start, 'statement runs to end of file');
      break;
    }
    if (term === '}') {
      unparsed(start, 'statement without a terminating semicolon');
      i = j;
      continue;
    }
    if (term === ';') {
      classify(start, j, ';');
      i = j + 1;
      continue;
    }
    if (term === '{scope') {
      const head = M.slice(start, j).replace(/\s+/g, ' ').trim();
      const core = stripLeadingTemplate(head).replace(/^(\[\[[^\]]*\]\]\s*)+/, '');
      const parent = top();
      const parentOpen = parent.kind !== 'class' || parent.pub;
      let sc;
      if (/namespace\b/.test(core) && !/^(class|struct|union)\b/.test(core)) {
        const nm = core
          .replace(/^inline\s+/, '')
          .replace(/^namespace\s*/, '')
          .trim();
        const hidden = nm === '' || /(^|::)(detail|internal|impl)$/.test(nm);
        sc = {
          kind: 'namespace',
          name: nm,
          reach: parent.reach && !hidden,
          conv: parent.conv || hidden,
        };
      } else if (/^extern\b/.test(core)) {
        sc = { kind: 'extern', name: '', reach: parent.reach, conv: parent.conv };
      } else {
        const isClassKw = core.match(/^(class|struct|union)\b/)[1];
        let h = core.replace(/^(class|struct|union)\s+/, '').replace(/\[\[[^\]]*\]\]/g, '');
        h = h
          .split(/(?<!:):(?!:)/)[0]
          .replace(/\bfinal\b/, '')
          .replace(/<.*$/, '')
          .trim();
        const nm = lastIdent(h);
        sc = {
          kind: 'class',
          name: nm,
          reach: parent.reach && parentOpen,
          conv: parent.conv,
          pub: isClassKw !== 'class',
          sect: null,
        };
        if (nm) {
          const endSig = M.slice(start, j).replace(/\s+$/, '').length + start;
          record(isClassKw === 'class' ? 'class' : isClassKw, nm, start, endSig);
        } else if (parent.reach && parentOpen) {
          unparsed(start, 'anonymous class or struct');
        }
        // the record above was made in the parent scope; the member scope starts now
      }
      stack.push({ ...sc, openIdx: j });
      i = j + 1;
      continue;
    }
    if (term === '{enum') {
      const close = matching(M, j, '{', '}');
      if (close < 0) {
        unparsed(start, 'enum without a closing brace');
        break;
      }
      const head = M.slice(start, j).replace(/\s+/g, ' ').trim();
      const nm = lastIdent(head.split(/(?<!:):(?!:)/)[0].trim());
      if (nm) record('enum', nm, start, close + 1);
      else if (top().reach) unparsed(start, 'anonymous enum');
      const semi = M.indexOf(';', close);
      i = semi > 0 && /^[\s\w*&,]*$/.test(M.slice(close + 1, semi)) ? semi + 1 : close + 1;
      continue;
    }
    if (term === '{func') {
      const close = matching(M, j, '{', '}');
      if (close < 0) {
        unparsed(start, 'function body without a closing brace');
        break;
      }
      let sigEnd = j;
      let headText = M.slice(start, j);
      const core = stripLeadingTemplate(headText.replace(/\s+/g, ' ').trim());
      const fp = findFuncParen(core);
      const cl = fp > 0 ? matching(core, fp, '(', ')') : -1;
      if (cl > 0) {
        const afterAbs = headText.indexOf(core.slice(cl + 1, cl + 4).trim() || ')');
        void afterAbs;
        // cut a constructor initializer list: the first lone ':' after the parameter list
        const origClose = (() => {
          const o = headText.indexOf('(', headText.search(/\S/));
          let q = findFuncParen(headText);
          if (q < 0) q = o;
          return matching(headText, q, '(', ')');
        })();
        if (origClose > 0) {
          const m = headText.slice(origClose + 1).match(/(^|[^:]):(?!:)/);
          if (m) sigEnd = start + origClose + 1 + m.index + m[1].length;
        }
      }
      classify(start, sigEnd, '{');
      i = close + 1;
      continue;
    }
    unparsed(start, 'brace block that is not a scope, enum or function');
    const close = matching(M, j, '{', '}');
    if (close < 0) break;
    i = close + 1;
  }
  if (stack.length > 1) unparsed(stack[stack.length - 1].openIdx ?? 0, 'scope is never closed');
  return out;
}

// ---------------------------------------------------------------------------------------------
// Nitro spec (TypeScript) scanner
// ---------------------------------------------------------------------------------------------
function scanTs(file, S) {
  const lx = lex(S, 'ts');
  const M = lx.M;
  const n = M.length;
  const out = { decls: [], unparsed: [], bannedSkipped: 0 };
  const unparsed = (idx, reason) => out.unparsed.push({ file, line: lx.lineOf(idx), reason });
  const add = (kind, name, cls, start, end) => {
    const line = lx.lineOf(start);
    const sig = printSig(lx, start, end);
    const doc = docFor(lx, line);
    if (banned.test(sig) || banned.test(name)) {
      out.bannedSkipped++;
      return;
    }
    out.decls.push({
      file,
      line,
      kind,
      name,
      ns: '',
      cls,
      sig,
      doc: doc && banned.test(doc) ? { omitted: true } : doc,
    });
  };
  const readStmt = (from, limit) => {
    let p = 0;
    let br = 0;
    let ang = 0;
    for (let j = from; j < limit; j++) {
      const c = M[j];
      if (c === '(') p++;
      else if (c === ')') p = Math.max(0, p - 1);
      else if (c === '[') br++;
      else if (c === ']') br = Math.max(0, br - 1);
      else if (c === '<') ang++;
      else if (c === '>' && M[j - 1] !== '=') ang = Math.max(0, ang - 1);
      else if (p === 0 && br === 0 && ang === 0 && (c === ';' || c === '{' || c === '}'))
        return { j, term: c };
    }
    return { j: limit, term: null };
  };
  let i = 0;
  while (i < n) {
    while (i < n && /\s/.test(M[i])) i++;
    if (i >= n) break;
    if (M[i] === ';') {
      i++;
      continue;
    }
    const start = i;
    const { j, term } = readStmt(i, n);
    const head = M.slice(start, j).replace(/\s+/g, ' ').trim();
    if (term === null || term === '}') {
      unparsed(start, 'statement without a terminator');
      i = Math.max(j + 1, start + 1);
      continue;
    }
    if (/^import\b/.test(head) || /^export\s*\{/.test(head) || /^export\s+\*/.test(head)) {
      i = term === '{' ? matching(M, j, '{', '}') + 1 || n : j + 1;
      if (term === '{') {
        const semi = M.indexOf(';', i);
        i = semi > 0 && semi - i < 200 ? semi + 1 : i;
      }
      continue;
    }
    const cm = head.match(/\b(class|interface)\s+([A-Za-z_$][\w$]*)/);
    if (term === '{' && cm) {
      const close = matching(M, j, '{', '}');
      if (close < 0) {
        unparsed(start, 'type body without a closing brace');
        break;
      }
      add(cm[1], cm[2], '', start, j);
      let k = j + 1;
      while (k < close) {
        while (k < close && /\s/.test(M[k])) k++;
        if (k >= close) break;
        if (M[k] === ';' || M[k] === ',') {
          k++;
          continue;
        }
        const r = readStmt(k, close);
        let end = r.j;
        let next = r.j + 1;
        if (r.term === '{') {
          const c2 = matching(M, r.j, '{', '}');
          end = c2 > 0 ? c2 + 1 : close;
          next = end;
          const semi = M.indexOf(';', end);
          if (semi > 0 && semi < close && /^\s*$/.test(M.slice(end, semi))) next = semi + 1;
        }
        const mh = M.slice(k, end).replace(/\s+/g, ' ').trim();
        const mm = mh.match(
          /^(?:(?:public|private|protected|static|readonly|abstract|declare|get|set)\s+)*([A-Za-z_$][\w$]*)\s*(\??)\s*([(:<])/
        );
        if (!mm) unparsed(k, 'class or interface member');
        else add(mm[3] === ':' ? 'property' : 'method', mm[1], cm[2], k, end);
        k = next;
      }
      i = close + 1;
      continue;
    }
    if (term === '{') {
      const close = matching(M, j, '{', '}');
      if (close < 0) {
        unparsed(start, 'block without a closing brace');
        break;
      }
      let end = close + 1;
      const semi = M.indexOf(';', end);
      if (semi > 0 && /^\s*$/.test(M.slice(end, semi))) end = semi + 1;
      const m = head.match(/\b(enum|type|namespace|function|const)\s+([A-Za-z_$][\w$]*)/);
      if (m) add(m[1] === 'const' ? 'variable' : m[1], m[2], '', start, end);
      else unparsed(start, 'top-level block');
      i = end;
      continue;
    }
    const m = head.match(/\b(function|type|const|let|var)\s+([A-Za-z_$][\w$]*)/);
    if (m)
      add(
        m[1] === 'function' ? 'function' : m[1] === 'type' ? 'type' : 'variable',
        m[2],
        '',
        start,
        j
      );
    else unparsed(start, 'top-level statement');
    i = j + 1;
  }
  return out;
}

// ---------------------------------------------------------------------------------------------
// Header classification. Each rule cites CMake evidence that is looked up in the file, so a
// renamed target or a moved line fails the generator instead of leaving a stale citation.
// ---------------------------------------------------------------------------------------------
const cmakeFiles = git('ls-files', '*CMakeLists.txt', '*.cmake')
  .split('\n')
  .filter(Boolean)
  .sort(cmp);
const cmakeText = new Map(cmakeFiles.map((f) => [f, read(f)]));
const line = (file, re) => ({ type: 'line', file, re });
const absent = (needle) => ({ type: 'absent', needle });
const absentIn = (file, needle) => ({ type: 'absentIn', file, needle });

function resolveEvidence(ev) {
  if (ev.type === 'line') {
    const text = cmakeText.get(ev.file) ?? read(ev.file);
    const lines = text.split('\n');
    const idx = lines.findIndex((l) => new RegExp(ev.re).test(l));
    if (idx < 0) throw new Error(`evidence not found: ${ev.file} /${ev.re}/`);
    return {
      md: `[${ev.file}#L${idx + 1}](${BLOB}${ev.file}#L${idx + 1}) \`${lines[idx].trim().replace(/\s+/g, ' ')}\``,
      plain: `${ev.file}:${idx + 1}`,
    };
  }
  if (ev.type === 'absent') {
    for (const [f, t] of cmakeText)
      if (t.includes(ev.needle))
        throw new Error(`evidence claim is false: ${f} mentions ${ev.needle}`);
    return { md: `no CMake file mentions \`${ev.needle}\``, plain: 'absent' };
  }
  const text = cmakeText.get(ev.file);
  if (text.includes(ev.needle))
    throw new Error(`evidence claim is false: ${ev.file} mentions ${ev.needle}`);
  return { md: `[${ev.file}](${BLOB}${ev.file}) has no \`${ev.needle}\``, plain: 'absent' };
}

const INC = 'vnext/include/react-native-qnx/';
const PUB = (needle) =>
  `PUBLIC include root, so every header under ${needle} is reachable by consumers`;
const PAGES = {
  runtime: [
    'Runtime headers',
    'Host runtime classes in react-native-qnx: React host, task queue, image loader, dev support and web socket modules.',
  ],
  fabric: [
    'Fabric headers',
    'Fabric mounting and bridge headers: mounting layer, touch dispatch, accessibility and per-component bridges.',
  ],
  views: [
    'View headers',
    'Qt Quick view factory, color parsing and modal and text input QML helpers.',
  ],
  'views-image': [
    'Image view headers',
    'Image decoding, caching, layout, effects and the expo-image item.',
  ],
  gesture: ['Gesture headers', 'Gesture handler core, bindings and the Qt gesture platform.'],
  reanimated: ['Reanimated headers', 'Reanimated platform adapter for Qt.'],
  worklets: ['Worklets headers', 'Worklets platform adapter, UI thread and frame bridge for Qt.'],
  lifecycle: ['Lifecycle headers', 'Qt application lifecycle observer.'],
  'window-chrome': ['Window chrome headers', 'Window chrome model, window host and chrome item.'],
  'app-identity': ['App identity headers', 'Application icon set and label fitting.'],
  'qt-graphics': ['Qt graphics headers', 'Qt graphics surface and its state.'],
  'qt-motion': ['Qt motion headers', 'Native Qt motion controller, transform and timing types.'],
  'qt-svg': ['Qt SVG headers', 'Qt SVG item and SVG document, path and transform validation.'],
  'qt-video': ['Qt video headers', 'Qt video item.'],
  rnsvg: [
    'react-native-svg renderer headers',
    'Model, items, path and renderer for react-native-svg elements.',
  ],
  'rnsvg-scene': [
    'react-native-svg scene headers',
    'Scene traversal and painting for react-native-svg.',
  ],
  'rnsvg-text': ['react-native-svg text headers', 'Text layout and painting for react-native-svg.'],
  'rnsvg-filter': ['react-native-svg filter headers', 'Filter primitives for react-native-svg.'],
  'rnsvg-image': [
    'react-native-svg image headers',
    'Image element loading and painting for react-native-svg.',
  ],
  'qnx-screen': ['QNX Screen headers', 'QNX Screen window-system surface.'],
  forge: ['Forge runtime headers', 'QNX Forge runtime adapter.'],
  'nitro-runtime': [
    'Nitro runtime headers',
    'C++ headers of the Nitro runtime package: installer, platform info and Qt dispatcher.',
  ],
  'expo-modules': ['Expo modules headers', 'C++ headers of the QNX Expo modules core package.'],
  'nitro-specs': [
    'Nitro specs',
    'TypeScript interface members of the Nitro specs, with their doc comments.',
  ],
};

const rules = [
  {
    re: /^tests\//,
    cls: 'internal',
    why: 'Test code and fixtures. Test targets use the library include roots as PRIVATE; no library exports this directory.',
    ev: [
      line(
        'tests/native/worklets/CMakeLists.txt',
        /target_include_directories\(rnqnx_worklets_lifecycle_test PRIVATE/.source
      ),
    ],
  },
  {
    re: /^vnext\/probes\//,
    cls: 'internal',
    why: 'Host probe sources. The only CMake use is a PRIVATE include of the playground host executable.',
    ev: [
      line(
        'CMakeLists.txt',
        /target_include_directories\(rnqnx_playground_host PRIVATE vnext\/probes\/playground\)/
          .source
      ),
    ],
  },
  {
    re: /^vnext\/src\//,
    cls: 'internal',
    why: 'Private source directory. The only CMake use is a PRIVATE include of a test target.',
    ev: [
      line(
        'cmake/RnqnxQtRnsvgFabric.cmake',
        /rnqnx_qt_rnsvg_values_test PRIVATE "\$\{root\}\/vnext\/src\/fabric"/.source
      ),
    ],
  },
  {
    re: /^vnext\/generated\//,
    cls: 'internal',
    why: 'Codegen output for the Fabric components. It is regenerated from the component specs, which are the contract.',
    ev: [
      line('cmake/RnqnxReactQt.cmake', /"\$\{RNQNX_REACT_QT_ROOT\}\/vnext\/generated"\)/.source),
    ],
  },
  {
    re: /^docs\/evidence\//,
    cls: 'internal',
    why: 'Captured evidence artifact. No CMake target uses it.',
    ev: [absent('docs/evidence')],
  },
  {
    re: /^cmake\/qnx-hermes-shims\//,
    cls: 'internal',
    why: 'libc header shim the QNX toolchain adds with -isystem. It patches the sysroot and is not an API.',
    ev: [line('cmake/toolchains/qnx-8.0.cmake', /qnx-hermes-shims/.source)],
  },
  {
    re: /^native\/platform\/linux-host\/(?!nitro\/)/,
    cls: 'internal',
    why: 'Linux host start-up helpers, included by the host executable and its tests. The adapter library next to them does not list or export these headers.',
    ev: [absentIn('native/platform/linux-host/nitro/CMakeLists.txt', 'MainThreadStack')],
  },
  {
    re: /^tools\/qnx-winmgr\//,
    cls: 'internal',
    why: 'Window manager tool source. No CMake target includes it.',
    ev: [absent('tools/qnx-winmgr')],
  },
  {
    re: /^native\/webgpu\//,
    cls: 'internal',
    why: 'WebGPU probe and test app. Its include directories are PRIVATE to the test executable.',
    ev: [
      line(
        'native/webgpu/CMakeLists.txt',
        /target_include_directories\(rnqnx_webgpu_test PRIVATE/.source
      ),
    ],
  },
  {
    re: /^packages\/expo-image-qnx\/cpp\//,
    cls: 'internal',
    why: 'The target declares no include directory, so these headers are reachable only from its own sources.',
    ev: [
      line('packages/expo-image-qnx/CMakeLists.txt', /add_library\(expo_image_qnx STATIC/.source),
      absentIn('packages/expo-image-qnx/CMakeLists.txt', 'target_include_directories'),
    ],
  },
  {
    re: /^packages\/qnx-nitro-runtime\/cpp\/NitroCoreCompat\.hpp$/,
    cls: 'internal',
    why: 'Force-included compatibility shim for the pinned Nitro alpha, passed as a PRIVATE compile option.',
    ev: [line('cmake/RnqnxNitroQt.cmake', /"-include" "\$\{RNQNX_NITRO_V1_COMPAT\}"/.source)],
  },
  {
    re: /^packages\/qnx-nitro-runtime\/cpp\/QtNitroDispatcher\.hpp$/,
    cls: 'public',
    page: 'nitro-runtime',
    why: `${PUB('packages/qnx-nitro-runtime/cpp')}.`,
    ev: [
      line('cmake/RnqnxNitroQt.cmake', /target_include_directories\(rnqnx_nitro_qt PUBLIC/.source),
    ],
  },
  {
    re: /^packages\/qnx-nitro-runtime\/cpp\//,
    cls: 'public',
    page: 'nitro-runtime',
    why: `${PUB('packages/qnx-nitro-runtime/cpp')}.`,
    ev: [
      line(
        'packages/qnx-nitro-runtime/CMakeLists.txt',
        /target_include_directories\(rnqnx_nitro_qnx PUBLIC/.source
      ),
    ],
  },
  {
    re: /^packages\/expo-modules-core-qnx\/cpp\//,
    cls: 'public',
    page: 'expo-modules',
    why: `${PUB('packages/expo-modules-core-qnx/cpp')}.`,
    ev: [
      line(
        'packages/expo-modules-core-qnx/CMakeLists.txt',
        /target_include_directories\(rnqnx_expo_modules_qnx PUBLIC/.source
      ),
    ],
  },
  {
    re: /^native\/platform\/qnx\/screen\//,
    cls: 'public',
    page: 'qnx-screen',
    why: `${PUB('native/platform/qnx')}.`,
    ev: [line('CMakeLists.txt', /target_include_directories\(qnx_platform PUBLIC/.source)],
  },
  {
    re: /^native\/platform\/qnx\/forge\//,
    cls: 'public',
    page: 'forge',
    why: `${PUB('native/platform/qnx/forge')}.`,
    ev: [
      line(
        'cmake/RnqnxForge.cmake',
        /target_include_directories\(rnqnx_forge_runtime PUBLIC/.source
      ),
    ],
  },
  {
    re: /^native\/lifecycle\//,
    cls: 'public',
    page: 'lifecycle',
    why: `${PUB('native/lifecycle')}.`,
    ev: [
      line(
        'native/lifecycle/CMakeLists.txt',
        /target_include_directories\(rnqnx_lifecycle_observer PUBLIC/.source
      ),
    ],
  },
  {
    re: /^native\/window-chrome\//,
    cls: 'public',
    page: 'window-chrome',
    why: `${PUB('native/window-chrome')}.`,
    ev: [
      line(
        'cmake/RnqnxWindowChrome.cmake',
        /target_include_directories\(\$\{target\} PUBLIC "\$\{root\}"/.source
      ),
    ],
  },
  {
    re: /^native\/app-identity\//,
    cls: 'public',
    page: 'app-identity',
    why: `${PUB('native/app-identity')}.`,
    ev: [
      line(
        'native/app-identity/CMakeLists.txt',
        /target_include_directories\(rnqnx_identity_core PUBLIC/.source
      ),
    ],
  },
  {
    re: /^native\/qt\/graphics\//,
    cls: 'public',
    page: 'qt-graphics',
    why: `${PUB('native/qt/graphics')}.`,
    ev: [
      line(
        'native/qt/graphics/CMakeLists.txt',
        /target_include_directories\(rnqnx_qt_graphics PUBLIC/.source
      ),
    ],
  },
  {
    re: /^native\/qt\/motion\//,
    cls: 'public',
    page: 'qt-motion',
    why: `${PUB('native/qt/motion')}.`,
    ev: [
      line(
        'native/qt/motion/CMakeLists.txt',
        /target_include_directories\(rnqnx_qt_motion PUBLIC/.source
      ),
    ],
  },
  {
    re: /^native\/qt\/svg\//,
    cls: 'public',
    page: 'qt-svg',
    why: `${PUB('native/qt/svg')}.`,
    ev: [
      line(
        'native/qt/svg/CMakeLists.txt',
        /target_include_directories\(rnqnx_qt_svg PUBLIC/.source
      ),
    ],
  },
  {
    re: /^native\/qt\/video\//,
    cls: 'public',
    page: 'qt-video',
    why: `${PUB('native/qt/video')}.`,
    ev: [
      line(
        'native/qt/video/CMakeLists.txt',
        /target_include_directories\(rnqnx_qt_video PUBLIC/.source
      ),
    ],
  },
  ...['scene', 'text', 'filter', 'image'].map((d) => ({
    re: new RegExp(`^native/qt/rnsvg/${d}/`),
    cls: 'public',
    page: `rnsvg-${d}`,
    why: `${PUB('native/qt/rnsvg')}, including the ${d}/ subdirectory.`,
    ev: [
      line(
        'native/qt/rnsvg/CMakeLists.txt',
        /target_include_directories\(rnqnx_qt_rnsvg PUBLIC/.source
      ),
    ],
  })),
  {
    re: /^native\/qt\/rnsvg\//,
    cls: 'public',
    page: 'rnsvg',
    why: `${PUB('native/qt/rnsvg')}.`,
    ev: [
      line(
        'native/qt/rnsvg/CMakeLists.txt',
        /target_include_directories\(rnqnx_qt_rnsvg PUBLIC/.source
      ),
    ],
  },
  {
    re: new RegExp(`^${INC}gesture/`),
    cls: 'public',
    page: 'gesture',
    why: `${PUB('vnext/include')}.`,
    ev: [
      line(
        'cmake/RnqnxGestureHandler.cmake',
        /^\s*target_include_directories\(rnqnx_gesture_handler\s*$/.source
      ),
    ],
  },
  {
    re: new RegExp(`^${INC}reanimated/`),
    cls: 'public',
    page: 'reanimated',
    why: `${PUB('vnext/include')}.`,
    ev: [
      line(
        'cmake/RnqnxReanimated.cmake',
        /^\s*target_include_directories\(rnqnx_reanimated\s*$/.source
      ),
    ],
  },
  {
    re: new RegExp(`^${INC}worklets/`),
    cls: 'public',
    page: 'worklets',
    why: `${PUB('vnext/include')}.`,
    ev: [
      line(
        'cmake/RnqnxWorklets.cmake',
        /^\s*target_include_directories\(rnqnx_worklets\s*$/.source
      ),
    ],
  },
  {
    re: new RegExp(`^${INC}runtime/`),
    cls: 'public',
    page: 'runtime',
    why: `${PUB('vnext/include')}.`,
    ev: [
      line('cmake/RnqnxReactQt.cmake', /target_include_directories\(rnqnx_views_qt PUBLIC/.source),
      line(
        'cmake/RnqnxRuntimeTrace.cmake',
        /target_include_directories\(rnqnx_runtime_trace PUBLIC/.source
      ),
    ],
  },
  {
    re: new RegExp(`^${INC}fabric/`),
    cls: 'public',
    page: 'fabric',
    why: `${PUB('vnext/include')}.`,
    ev: [
      line('cmake/RnqnxReactQt.cmake', /target_include_directories\(rnqnx_views_qt PUBLIC/.source),
    ],
  },
  {
    re: new RegExp(`^${INC}views/image/`),
    cls: 'public',
    page: 'views-image',
    why: `${PUB('vnext/include')}.`,
    ev: [
      line('cmake/RnqnxReactQt.cmake', /target_include_directories\(rnqnx_views_qt PUBLIC/.source),
    ],
  },
  {
    re: new RegExp(`^${INC}views/`),
    cls: 'public',
    page: 'views',
    why: `${PUB('vnext/include')}.`,
    ev: [
      line('cmake/RnqnxReactQt.cmake', /target_include_directories\(rnqnx_views_qt PUBLIC/.source),
    ],
  },
];

// install() commands: report how many exist and whether any installs a header.
const installLines = [];
for (const [f, t] of cmakeText)
  t.split('\n').forEach((l, k) => {
    if (/^\s*install\(/.test(l)) installLines.push({ f, k: k + 1, l });
  });
const installsHeader = installLines.filter((x) =>
  /\.(h|hpp)\b|DIRECTORY|FILE_SET|HEADERS/i.test(x.l)
).length;

// ---------------------------------------------------------------------------------------------
// Collect
// ---------------------------------------------------------------------------------------------
const allHeaders = git('ls-files', '*.h', '*.hpp').split('\n').filter(Boolean).sort(cmp);
const keptHeaders = allHeaders.filter((p) => !banned.test(p));
const skippedHeaders = allHeaders.length - keptHeaders.length;
const evCache = new Map();
const evFor = (r) => {
  if (!evCache.has(r)) evCache.set(r, r.ev.map(resolveEvidence));
  return evCache.get(r);
};

const headers = keptHeaders.map((path) => {
  const rule = rules.find((r) => r.re.test(path));
  if (!rule) throw new Error(`no classification rule for header ${path}`);
  return { path, rule, cls: rule.cls, page: rule.page ?? null };
});

const results = new Map(); // page -> { files: Map(path -> decls) }
const notParsed = [];
let convSkipped = 0;
let bannedSkippedDecls = 0;
for (const h of headers) {
  if (h.cls !== 'public') continue;
  const r = scanCpp(h.path, read(h.path));
  h.decls = r.decls;
  convSkipped += r.convSkipped;
  bannedSkippedDecls += r.bannedSkipped;
  notParsed.push(...r.unparsed);
  if (!results.has(h.page)) results.set(h.page, new Map());
  results.get(h.page).set(h.path, r.decls);
}

const specFiles = git('ls-files', '*.nitro.ts').split('\n').filter(Boolean).sort(cmp);
const keptSpecs = specFiles.filter((p) => !banned.test(p));
const skippedSpecs = specFiles.length - keptSpecs.length;
const specResults = new Map();
let specOmittedDocs = 0;
for (const f of keptSpecs) {
  const r = scanTs(f, read(f));
  bannedSkippedDecls += r.bannedSkipped;
  notParsed.push(...r.unparsed);
  specResults.set(f, r.decls);
  for (const d of r.decls) if (d.doc?.omitted) specOmittedDocs++;
}
notParsed.sort((a, b) => cmp(a.file, b.file) || a.line - b.line || cmp(a.reason, b.reason));

// ---------------------------------------------------------------------------------------------
// Render
// ---------------------------------------------------------------------------------------------
const qname = (d, spec) =>
  (spec
    ? [d.cls, d.name].filter(Boolean).join('.')
    : [d.ns, d.cls, d.name].filter(Boolean).join('::')) || d.name;
const front = (title, description) => [
  '---',
  `title: ${JSON.stringify(title)}`,
  `description: ${JSON.stringify(description)}`,
  '---',
  '',
  '{/* Generated by docs-site/scripts/generate-api.mjs. Do not edit by hand. */}',
  '',
];

function renderDecl(d, spec) {
  const out = [`### \`${qname(d, spec)}\``, ''];
  const meta = [`Kind: ${d.kind}`];
  if (d.ns) meta.push(`Namespace: \`${d.ns}\``);
  if (d.cls) meta.push(`${spec ? 'Type' : 'Class'}: \`${d.cls}\``);
  meta.push(`Source: ${srcLink(d.file, d.line)}`);
  out.push(meta.join(' · '), '', fence(d.sig, spec ? 'ts' : 'cpp'), '');
  if (d.doc && d.doc.omitted) out.push(BANNED_DOC_NOTICE, '');
  else if (d.doc) out.push('Doc comment:', '', fence(d.doc, 'text'), '');
  else out.push(NO_DOC, '');
  return out;
}

const outputs = {};
const counts = {};
const pageOrder = Object.keys(PAGES)
  .filter((k) => k !== 'nitro-specs' && results.has(k))
  .sort(cmp);
for (const key of pageOrder) {
  const [title, desc] = PAGES[key];
  const files = [...results.get(key).keys()].sort(cmp);
  const total = files.reduce((a, f) => a + results.get(key).get(f).length, 0);
  counts[key] = total;
  const body = [
    ...front(title, desc),
    `${total} declarations from ${files.length} headers. Signatures and doc comments come from the headers as written. See the [index](${DOCS_URL}) for how headers are classified and what the scanner could not parse.`,
    '',
  ];
  for (const f of files) {
    const ds = results.get(key).get(f);
    body.push(`## \`${f}\``, '', `Header: [${f}](${BLOB}${f}). ${ds.length} declarations.`, '');
    if (!ds.length) body.push('The scanner found no public declarations in this header.', '');
    for (const d of ds) body.push(...renderDecl(d, false));
  }
  outputs[`${OUT_DIR}/${key}.mdx`] =
    body
      .join('\n')
      .replace(/\n{3,}/g, '\n\n')
      .trimEnd() + '\n';
}

// Nitro specs
{
  const [title, desc] = PAGES['nitro-specs'];
  const total = [...specResults.values()].reduce((a, d) => a + d.length, 0);
  counts['nitro-specs'] = total;
  const body = [
    ...front(title, desc),
    `${total} declarations from ${specResults.size} spec ${specResults.size === 1 ? 'file' : 'files'}. A \`*.nitro.ts\` file is the input to nitrogen, which generates the C++ and JavaScript bindings. These are the TypeScript members as written, with their doc comments.`,
    '',
  ];
  for (const f of [...specResults.keys()].sort(cmp)) {
    body.push(
      `## \`${f}\``,
      '',
      `Spec: [${f}](${BLOB}${f}). ${specResults.get(f).length} declarations.`,
      ''
    );
    for (const d of specResults.get(f)) body.push(...renderDecl(d, true));
  }
  outputs[`${OUT_DIR}/nitro-specs.mdx`] =
    body
      .join('\n')
      .replace(/\n{3,}/g, '\n\n')
      .trimEnd() + '\n';
}

// Index
const publicHeaders = headers.filter((h) => h.cls === 'public');
const internalHeaders = headers.filter((h) => h.cls === 'internal');
const totalDecls = Object.values(counts).reduce((a, b) => a + b, 0);
const pageLink = (k) => `[${PAGES[k][0]}](${DOCS_URL}/${k})`;
const idx = [
  ...front(
    'Native API reference',
    'Per-function reference for the public C++ headers and the Nitro specs, generated from the source with a deterministic scanner.'
  ),
  `This section lists the declarations an app or module author can call from the public C++ headers of react-native-qnx and from the Nitro specs. Each entry has the name, signature, namespace, class, the doc comment copied from the header, and a link to the line in \`main\`. Where a header has no comment, the entry says so. No description is written by hand or inferred.`,
  '',
  '## How it is generated',
  '',
  '`node docs-site/scripts/generate-api.mjs` writes these pages. `node docs-site/scripts/generate-api.mjs --check` exits 1 when a committed page differs from what the generator would write. The output has no timestamps, absolute paths or commit SHAs, and every list is sorted, so two runs over the same tree are byte-identical.',
  '',
  'Inputs are the tracked `*.h` and `*.hpp` files and the tracked `*.nitro.ts` files. The C++ scanner is a small brace-depth scanner, not a C++ compiler front end. It reads namespaces, classes, structs, enums, aliases, functions, methods, constructors, signals, slots, `Q_PROPERTY` declarations, fields, variables and function-like macros. Preprocessor conditions are not evaluated, so both branches of an `#if` are read.',
  '',
  'Rules the scanner applies:',
  '',
  '- A doc comment is the run of `///`, `//` or `/* */` comment lines that ends on the line directly above the declaration, with no blank line between. The text is copied with the comment markers removed.',
  '- Em and en dashes in comments are written as a hyphen.',
  '- In a class, only `public` members, signals and slots are listed, plus every `Q_PROPERTY` because QML reads those whatever the C++ access section says. `private` and `protected` members are not listed.',
  '- Anonymous namespaces and namespaces named `detail`, `internal` or `impl` are internal by convention and are not expanded. Deleted functions and forward declarations are not listed.',
  '- A doc comment that contains a term the docs build bans is replaced by a notice. A declaration whose own text contains the term is left out and counted in the totals below.',
  '',
  '## Public and internal headers',
  '',
  `No CMake file installs a header: of ${installLines.length} \`install()\` commands, ${installsHeader} install a header. They install the consumer app icons, executable and bundle. So the public/internal split follows the include paths. A header is **public** when it sits under a directory that a library target exports with a \`PUBLIC\` \`target_include_directories\`, because any target that links the library can then include it. A header is **internal** when its directory is only a \`PRIVATE\` include of a test or probe target, is generated, is a toolchain shim, or no CMake file references it. Each row below cites the CMake line that backs its classification; the generator fails if a cited line is missing or a claimed absence is false.`,
  '',
  `Totals: ${headers.length} headers listed (${publicHeaders.length} public, ${internalHeaders.length} internal), ${totalDecls} declarations extracted. ${skippedHeaders + skippedSpecs} ${skippedHeaders + skippedSpecs === 1 ? 'file is' : 'files are'} skipped because the path contains a term the docs build bans; they are counted here and not listed. ${bannedSkippedDecls} ${bannedSkippedDecls === 1 ? 'declaration was' : 'declarations were'} left out for the same reason. ${specOmittedDocs} spec doc ${specOmittedDocs === 1 ? 'comment is' : 'comments are'} replaced by a notice. ${convSkipped} declarations in internal-by-convention namespaces are not expanded.`,
  '',
  '## Not parsed',
  '',
  `Not parsed: ${notParsed.length}`,
  '',
  notParsed.length
    ? 'The scanner could not classify these statements. They are not in the pages. Each is a gap in this reference until the scanner or the header changes.'
    : 'The scanner classified every statement it read in the public headers and specs. There is no gap to list.',
  '',
  ...(notParsed.length
    ? [
        '| File | Line | Reason |',
        '| --- | --- | --- |',
        ...notParsed.map(
          (u) => `| [${esc(u.file)}](${BLOB}${u.file}#L${u.line}) | ${u.line} | ${esc(u.reason)} |`
        ),
        '',
      ]
    : []),
  '## Pages',
  '',
  '| Page | Declarations | Headers |',
  '| --- | --- | --- |',
  ...[...pageOrder, 'nitro-specs'].map((k) => {
    const hc = k === 'nitro-specs' ? specResults.size : results.get(k).size;
    return `| ${pageLink(k)} | ${counts[k]} | ${hc} |`;
  }),
  '',
  '## Every header',
  '',
  '| Header | Class | Page | Declarations | Evidence and reason |',
  '| --- | --- | --- | --- | --- |',
  ...headers.map((h) => {
    const ev = evFor(h.rule)
      .map((e) => e.md)
      .join('; ');
    const page = h.page ? pageLink(h.page) : 'not expanded';
    const dc = h.cls === 'public' ? String(h.decls.length) : '-';
    return `| [${esc(h.path)}](${BLOB}${h.path}) | ${h.cls} | ${page} | ${dc} | ${esc(h.rule.why)} ${ev} |`;
  }),
  '',
];
outputs[`${OUT_DIR}/index.mdx`] =
  idx
    .join('\n')
    .replace(/\n{3,}/g, '\n\n')
    .trimEnd() + '\n';

outputs[`${OUT_DIR}/meta.json`] =
  JSON.stringify({ title: 'Native API', pages: ['index', ...pageOrder, 'nitro-specs'] }, null, 2) +
  '\n';

// ---------------------------------------------------------------------------------------------
// Write or check
// ---------------------------------------------------------------------------------------------
for (const [rel, body] of Object.entries(outputs))
  if (banned.test(body)) throw new Error(`banned term in generated ${rel}`);

const outAbs = join(site, OUT_DIR);
const existing = existsSync(outAbs) ? readdirSync(outAbs).map((f) => `${OUT_DIR}/${f}`) : [];
for (const rel of existing.filter((f) => !(f in outputs)).sort(cmp)) {
  if (check) {
    console.error(`stale (no longer generated): ${rel}`);
    process.exitCode = 1;
  } else {
    unlinkSync(join(site, rel));
    console.log(`removed ${rel}`);
  }
}
for (const [rel, body] of Object.entries(outputs)) {
  const abs = join(site, rel);
  if (check) {
    if (!existsSync(abs) || readFileSync(abs, 'utf8') !== body) {
      console.error(`stale: ${rel}`);
      process.exitCode = 1;
    }
  } else {
    mkdirSync(dirname(abs), { recursive: true });
    writeFileSync(abs, body);
    console.log(`wrote ${rel}`);
  }
}
if (!check || !process.exitCode) {
  const summary = [...pageOrder, 'nitro-specs'].map((k) => `${k}=${counts[k]}`).join(' ');
  console.log(
    `headers: ${headers.length} (public ${publicHeaders.length}, internal ${internalHeaders.length}); declarations: ${totalDecls}; not parsed: ${notParsed.length}`
  );
  console.log(summary);
}
