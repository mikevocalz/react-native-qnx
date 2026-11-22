#!/usr/bin/env python3
"""Assemble a provisional Qt runtime receipt from checked files, never release approval.

Usage: python3 -I generate-package-manifest.py --runtime DIR --source-manifest FILE
       --patch-stamp FILE --stages FILE --build-plugin FILE --output NEW_FILE
"""
import argparse
import csv
import datetime
import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import re
import subprocess
import sys
import tempfile

PATCHES = ['qtbase-qnx-offscreen-surface.patch', 'qtbase-qnx-resize-swap.patch']
SOFTWARE = 'qtbase-qnx-software-grab.patch'
STAGES = ['extract', 'cve-patches', 'qnx-patches', 'host-tools', 'qnx-core', 'qtsvg', 'qtimageformats']
HEADER = ['stage', 'start_utc', 'end_utc', 'wall_s', 'exit', 'work_dir_kib_after', 'free_kib_after']
ARCHIVE_SHA = '98ff4f44bac6ec3e1e62ee2a4316ae0e3d15badb015d753cc0268a20db52f165'
CHECKS = {'sums', 'manifest', 'needed', 'qt-closed', 'rpath', 'no-sdp', 'notices'}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def safe_relative(name):
    require(isinstance(name, str) and name and '\\' not in name,
            'invalid relative path')
    value = PurePosixPath(name)
    require(not value.is_absolute() and all(p not in ('', '.', '..') for p in name.split('/')),
            f'unsafe relative path: {name}')
    return value


def no_symlink_components(path):
    # macOS supplies these filesystem aliases; other user-supplied symlink
    # components would obscure which build/source evidence was inspected.
    aliases = {Path('/tmp'), Path('/var'), Path('/etc')}
    absolute = path.absolute()
    for candidate in (absolute, *absolute.parents):
        require(not candidate.is_symlink() or candidate in aliases,
                f'symlink input/output path refused: {candidate.name}')


def ordinary(path):
    no_symlink_components(path)
    require(not path.is_symlink() and path.is_file(), f'expected regular non-symlink file: {path.name}')
    return path.resolve()


def contained(root, name):
    relative = safe_relative(name)
    path = root.joinpath(*relative.parts)
    current = root
    for part in relative.parts:
        current /= part
        require(not current.is_symlink(), f'symlink in metadata path: {name}')
    require(path.resolve().is_relative_to(root), f'path escaped root: {name}')
    return ordinary(path)


def pairs(items):
    result = {}
    for key, value in items:
        require(key not in result, f'duplicate JSON key: {key}')
        result[key] = value
    return result


def read_json(path):
    return json.loads(ordinary(path).read_text(), object_pairs_hook=pairs)


def inventory(root):
    require(root.is_dir() and not root.is_symlink(), 'runtime must be a real directory')
    files, links = {}, {}
    for parent, directories, names in os.walk(root, followlinks=False):
        for name in directories:
            require(not (Path(parent) / name).is_symlink(), 'runtime directory symlink refused')
        for name in names:
            path = Path(parent) / name
            rel = path.relative_to(root).as_posix()
            safe_relative(rel)
            if path.is_symlink():
                target = os.readlink(path)
                require(not PurePosixPath(target).is_absolute() and '\\' not in target,
                        f'unsafe runtime symlink: {rel}')
                require(path.resolve().is_relative_to(root) and path.is_file(),
                        f'escaping or dangling runtime symlink: {rel}')
                links[rel] = target
            else:
                files[rel] = digest(ordinary(path))
    return {'files': dict(sorted(files.items())), 'symlinks': dict(sorted(links.items()))}


def stage_sequence(path):
    reader = csv.DictReader(io.StringIO(ordinary(path).read_text()), delimiter='\t')
    require(reader.fieldnames == HEADER, 'invalid stages.tsv header')
    rows = list(reader)
    require([row['stage'] for row in rows] == STAGES, 'missing, duplicate or reordered stage')
    previous_end = None
    for row in rows:
        require(None not in row and all(value is not None for value in row.values()), 'malformed stage row')
        require(row['exit'] == '0', f'failed stage: {row["stage"]}')
        start, end = (datetime.datetime.strptime(row[key], '%Y-%m-%dT%H:%M:%SZ')
                      for key in ('start_utc', 'end_utc'))
        require(end >= start and (previous_end is None or start >= previous_end), 'overlapping stage times')
        for key in ('wall_s', 'work_dir_kib_after', 'free_kib_after'):
            require(re.fullmatch(r'\d+', row[key]) is not None, f'invalid {key}')
        require(abs((end - start).total_seconds() - int(row['wall_s'])) <= 1, 'stage duration mismatch')
        previous_end = end
    return rows


def source_inputs(manifest_path, stamp):
    source = read_json(manifest_path)
    require(source.get('schema') == 1 and source.get('qt', {}).get('version') == '6.12.0', 'unsupported source manifest')
    require(source['qt'].get('sha256') == ARCHIVE_SHA, 'unexpected Qt source archive hash')
    selection = source['rnQnxPatches']
    require(type(selection.get('softwareGrab')) is bool, 'softwareGrab must be a boolean')
    applied = PATCHES + ([SOFTWARE] if selection['softwareGrab'] else [])
    absent = [] if selection['softwareGrab'] else [SOFTWARE]
    require(selection.get('appliedByScript') == applied and selection.get('notAppliedByScript') == absent,
            'patch list disagrees with softwareGrab')
    require(ordinary(stamp).read_text().splitlines() == applied, 'source patch stamp disagrees with manifest')
    recipe = source['recipe']
    require(re.fullmatch(r'[a-f0-9]{40}', recipe.get('commit', '')) is not None, 'source commit must be full SHA')
    root = manifest_path.parent.resolve()
    hashes = {str(ordinary(manifest_path)): digest(manifest_path), str(ordinary(stamp)): digest(stamp)}
    included = source['qt'].get('includedInBundle', False)
    require(type(included) is bool, 'includedInBundle must be a boolean')
    if included:
        archive = contained(root, source['qt'].get('archive', 'qt-everywhere-src-6.12.0.tar.xz'))
        require(digest(archive) == ARCHIVE_SHA, 'included Qt archive hash mismatch')
        hashes[str(archive)] = ARCHIVE_SHA
    for group in (recipe['files'], source['appliedQtbaseDiffs']['files']):
        require(isinstance(group, dict) and group, 'empty source file inventory')
        for name, expected in group.items():
            require(isinstance(expected, str) and re.fullmatch(r'[a-f0-9]{64}', expected) is not None, 'invalid source hash')
            path = contained(root, name)
            require(digest(path) == expected, f'source hash mismatch: {name}')
            hashes[str(path)] = expected
    for patch in applied:
        require(f'qt-public/patches/{patch}' in recipe['files'], f'missing applied patch: {patch}')
    return source, applied, absent, hashes


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('runtime', 'source-manifest', 'patch-stamp', 'stages', 'build-plugin', 'output'):
        parser.add_argument(f'--{name}', required=True, type=Path)
    args = parser.parse_args(argv)
    no_symlink_components(args.runtime)
    no_symlink_components(args.output)
    require(not args.runtime.is_symlink(), 'runtime symlink refused')
    runtime = args.runtime.resolve()
    source_manifest = ordinary(args.source_manifest)
    stamp, stages, build_plugin = (ordinary(path) for path in (args.patch_stamp, args.stages, args.build_plugin))
    output = args.output.parent.resolve() / args.output.name
    require(output.parent.is_dir() and not output.exists() and not output.is_symlink(), 'output must be new with an existing parent')
    require(not output.is_relative_to(runtime) and not output.is_relative_to(source_manifest.parent), 'output cannot modify runtime or source bundle')
    before = inventory(runtime)
    source, applied, absent, source_hashes = source_inputs(source_manifest, stamp)
    rows = stage_sequence(stages)
    source_hashes[str(stages)] = digest(stages)
    plugin = contained(runtime, 'plugins/platforms/libqqnx.so')
    plugin_hash = digest(plugin)
    require(plugin_hash == digest(build_plugin), 'staged plugin differs from build plugin')
    source_hashes[str(build_plugin)] = plugin_hash
    header = plugin.read_bytes()[:20]
    require(len(header) == 20 and header[:6] == b'\x7fELF\x02\x01' and header[18:20] == b'\xb7\x00', 'plugin must be ELF64 little-endian aarch64')
    # Reject unsafe paths before the closure verifier can follow JSON/sums entries.
    staged = read_json(contained(runtime, 'manifest.json'))
    for name in staged:
        safe_relative(name)
    require(set(staged) == (set(before['files']) - {'manifest.json', 'needed.json', 'SHA256SUMS'}) | set(before['symlinks']), 'staging manifest does not cover the runtime')
    for name, meta in staged.items():
        if name in before['symlinks']:
            require(meta == {'symlink': before['symlinks'][name]}, f'staging symlink mismatch: {name}')
        else:
            require(meta.get('sha256') == before['files'][name] and meta.get('bytes') == contained(runtime, name).stat().st_size, f'staging file mismatch: {name}')
    needed = read_json(contained(runtime, 'needed.json'))
    listed = {}
    for line in contained(runtime, 'SHA256SUMS').read_text().splitlines():
        require(re.fullmatch(r'[a-f0-9]{64} [ *].+', line) is not None, 'invalid SHA256SUMS row')
        name = line[66:].removeprefix('./')
        safe_relative(name)
        require(name not in listed, 'duplicate SHA256SUMS path')
        listed[name] = line[:64]
    require(listed == {name: sha for name, sha in before['files'].items() if name != 'SHA256SUMS'}, 'SHA256SUMS does not exactly match regular runtime files')
    verifier = Path(__file__).with_name('verify-closure.py')
    verifier_hash = digest(ordinary(verifier))
    with tempfile.TemporaryDirectory(prefix='qt-package-closure-') as temporary:
        report_path = Path(temporary) / 'closure.json'
        result = subprocess.run([sys.executable, '-I', str(verifier), str(runtime), '--json', str(report_path), '--runpath-prefix', '/data/qt612-public/'], capture_output=True, text=True, check=False)
        require(result.returncode == 0, f'fresh closure verification failed:\n{result.stdout}\n{result.stderr}')
        closure = read_json(report_path)
    require(closure.get('pass') is True and set(closure.get('checks', {})) == CHECKS and all(value.get('pass') is True for value in closure['checks'].values()), 'incomplete closure result')
    for name, meta in closure['files'].items():
        if re.search(r'\.so(?:\.\d+)*$', name):
            require('needed' in meta, f'library/plugin was not recognized as ELF64 little-endian: {name}')
        if 'needed' in meta:
            header = contained(runtime, name).read_bytes()[:20]
            require(len(header) == 20 and header[:6] == b'\x7fELF\x02\x01' and header[18:20] == b'\xb7\x00', f'wrong runtime ELF architecture: {name}')
            require(all(value == '/data/qt612-public/lib' for value in meta['runpath']), f'unexpected runtime search path: {name}')
    require(needed.get('needed') == {name: meta['needed'] for name, meta in closure['files'].items() if 'needed' in meta}, 'saved dependency inventory disagrees with fresh ELF scan')
    require(before == inventory(runtime), 'runtime changed during verification')
    require(digest(ordinary(verifier)) == verifier_hash, 'closure verifier changed during verification')
    require(all(digest(ordinary(Path(path))) == expected for path, expected in source_hashes.items()), 'source/build evidence changed during verification')
    files, links = before['files'], before['symlinks']
    manifest = {
        'schema': 1, 'unit': 'qt-runtime-qnx-aarch64le', 'qt': '6.12.0',
        'target': 'QNX SDP 8.0.5, aarch64le', 'status': 'provisional', 'releaseEligible': False,
        'notEligibleBecause': ['Owner source-offer approval and contact are still required.', 'Release units and corresponding source must be published.', 'Independent Verifier reproduction is still required.', 'A stage table verifies sequence and completion; it does not attest a pristine starting tree or compiled-source provenance.', 'The named target/plugin still requires its own qualification.'],
        'sourceCommit': source['recipe']['commit'], 'softwareGrab': source['rnQnxPatches']['softwareGrab'],
        'patchesApplied': applied, 'patchesNotApplied': absent, 'pluginSha256': plugin_hash,
        'sourceManifestSha256': source_hashes[str(source_manifest)], 'patchStampSha256': source_hashes[str(stamp)],
        'buildStages': {'sha256': source_hashes[str(stages)], 'sequenceVerified': True, 'cleanStartAttested': False, 'rows': rows},
        'counts': {'entries': len(files) + len(links), 'hashedFiles': len(files), 'symlinks': len(links), 'licenseFiles': sum(name.startswith('licenses/') for name in files)},
        'closureCheck': {'script': 'scripts/qt-public/verify-closure.py', 'verifierSha256': verifier_hash, 'runpathPrefix': '/data/qt612-public/', 'result': 'pass', 'checks': closure['checks']},
        'systemLibrariesUsed': closure['system_libraries_used'], 'notices': 'licenses/THIRD_PARTY_ATTRIBUTIONS.txt',
        'sourceOffer': 'docs/distribution/source-offer.md', 'files': files, 'symlinks': links,
    }
    with output.open('x') as stream:
        stream.write(json.dumps(manifest, indent=2, sort_keys=True) + '\n')
    print(f'Wrote provisional manifest: {len(files)} hashed files; releaseEligible=false')


if __name__ == '__main__':
    try:
        main()
    except (ValueError, KeyError, OSError, TypeError, AttributeError, csv.Error) as error:
        sys.exit(f'package manifest refused: {error}')
