#!/usr/bin/env python3
"""List the SPDX expression of every source file that ninja compiled for the
selected QNX targets, and flag files whose expression offers no LGPL option.

Usage: spdx-audit.py <ninja-build-dir> <source-root> <out.json> <target>...
Reads `ninja -t commands <target>...`, takes the `-c <source>` argument of
every compile command, and reads the SPDX header in each source's first 40
lines. Generated files outside <source-root> are counted, not inspected.
"""
import json, re, subprocess, sys
from pathlib import Path

build, src, out = Path(sys.argv[1]), Path(sys.argv[2]).resolve(), Path(sys.argv[3])
targets = sys.argv[4:]
cmds = subprocess.run(['ninja', '-C', str(build), '-t', 'commands', *targets],
                      check=True, capture_output=True, text=True).stdout.splitlines()
srcs = set()
for c in cmds:
    m = re.search(r'\s-c\s+(\S+)', c)
    if m:
        p = Path(m.group(1))
        if not p.is_absolute():
            p = (build / p)
        srcs.add(p.resolve())
spdx_re = re.compile(r'SPDX-License-Identifier:\s*(.+?)\s*(\*/)?$')
rows, flagged, generated = [], [], []
for p in sorted(srcs):
    try:
        rel = str(p.relative_to(src))
    except ValueError:
        generated.append(str(p)); continue
    expr = None
    with open(p, errors='replace') as f:
        for i, line in enumerate(f):
            m = spdx_re.search(line)
            if m:
                expr = m.group(1).strip(); break
            if i > 40: break
    rows.append({'file': rel, 'spdx': expr})
    if expr is None or 'LGPL' not in expr:
        flagged.append({'file': rel, 'spdx': expr})
summary = {}
for r in rows:
    summary[r['spdx'] or '(none in first 40 lines)'] = summary.get(r['spdx'] or '(none in first 40 lines)', 0) + 1
out.write_text(json.dumps({'targets': targets, 'compiled_source_files': len(rows),
    'generated_or_out_of_tree': len(generated), 'by_expression': summary,
    'no_lgpl_option': flagged}, indent=2) + '\n')
print(json.dumps(summary, indent=2)); print('no LGPL option:', len(flagged))
