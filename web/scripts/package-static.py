#!/usr/bin/env python3
"""Assemble a content-addressed development distribution; never a parity approval."""
import argparse
import hashlib
import importlib.util
from html.parser import HTMLParser
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def source_tools():
    spec = importlib.util.spec_from_file_location('surge_source_archive', ROOT / 'web/scripts/source-archive.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def file_records(files):
    return {name: {'size': path.stat().st_size, 'sha256': digest(path)} for name, path in sorted(files.items())}


def check_build_binding(files, archive, receipt):
    snapshot, source = source_tools().verify(archive)
    if (receipt.get('schema') != 1 or receipt.get('kind') != 'surge-browser-isolated-build'
            or receipt.get('snapshot') != snapshot or receipt.get('sourceCommit') != source['commit']
            or receipt.get('sourceTreeVerifiedBeforeAndAfter') is not True):
        raise ValueError('Build receipt does not identify the verified source snapshot')
    if receipt.get('outputs') != file_records(files):
        raise ValueError('Build outputs do not match the source build receipt')
    return snapshot, source


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as source:
        for block in iter(lambda: source.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def local_file(root, name):
    relative = PurePosixPath(name)
    if not name or relative.is_absolute() or '..' in relative.parts or '\\' in name or ':' in name:
        raise ValueError(f'Invalid package path: {name}')
    path = root / relative
    if any(p.is_symlink() for p in [path, *path.parents] if p != root.parent):
        raise ValueError(f'Symlink in package path: {name}')
    if not path.is_file() or not path.resolve().is_relative_to(root.resolve()):
        raise ValueError(f'Missing package file: {name}')
    return path


class Scripts(HTMLParser):
    def __init__(self):
        super().__init__()
        self.sources = []

    def handle_starttag(self, tag, attrs):
        if tag == 'script':
            source = dict(attrs).get('src')
            if source:
                self.sources.append(source)


def inputs(build):
    files = {}
    def add(name):
        path = local_file(build, name)
        files[name] = path
        return path
    html = add('surge-xt-browser.html')
    parser = Scripts()
    parser.feed(html.read_text())
    if 'surge-xt-browser.js' not in parser.sources:
        raise ValueError('Application HTML does not load surge-xt-browser.js')
    for name in parser.sources:
        add(name)
    for name in ['surge-xt-browser.wasm', 'surge-xt-browser.data', 'library.js']:
        add(name)
    # Include any SDK-generated worker sidecars, without diagnostic harnesses.
    for path in sorted(build.glob('surge-xt-browser.*.js')):
        add(path.name)
    manifest = json.loads(add('library/manifest.json').read_text())
    if manifest.get('schema') != 1 or not manifest.get('entries') or not manifest.get('patchIndex'):
        raise ValueError('Missing factory library inventory or patch index')
    for item in [*manifest['entries'], manifest['patchIndex']]:
        sha = item.get('sha256', '')
        if not re.fullmatch('[0-9a-f]{64}', sha) or item.get('url') != 'objects/' + sha:
            raise ValueError('Invalid factory object reference')
        path = add('library/' + item['url'])
        if path.stat().st_size != item['size'] or digest(path) != sha:
            raise ValueError(f'Factory object does not match manifest: {item["path"]}')
    return files


def verify(directory, version=None):
    manifest_path = local_file(directory, 'distribution.json')
    raw = manifest_path.read_bytes()
    manifest = json.loads(raw)
    if manifest.get('schema') != 1 or manifest.get('status') != 'development' or manifest.get('permissions') != 'files:0644,directories:0755':
        raise ValueError('Unsupported distribution manifest')
    if hashlib.sha256(raw).hexdigest() != (version or directory.name):
        raise ValueError('Distribution directory is not its manifest digest')
    expected = set(manifest['files']) | {'distribution.json'}
    actual = {p.relative_to(directory).as_posix() for p in directory.rglob('*') if p.is_file() or p.is_symlink()}
    if actual != expected:
        raise ValueError('Distribution contains missing or unlisted files')
    if os.name == 'posix':
        for path in [directory, *directory.rglob('*')]:
            expected_mode = 0o755 if path.is_dir() else 0o644
            if path.stat().st_mode & 0o777 != expected_mode:
                raise ValueError(f'Distribution permissions differ: {path.name}')
    for name, info in manifest['files'].items():
        path = local_file(directory, name)
        if path.stat().st_size != info['size'] or digest(path) != info['sha256']:
            raise ValueError(f'Distribution integrity failure: {name}')
    source = manifest['source']
    if source.get('snapshotArchiveIncluded'):
        receipt = json.loads(local_file(directory, 'build-receipt.json').read_text())
        snapshot, original = check_build_binding(inputs(directory), local_file(directory, source['archive']), receipt)
        if source.get('snapshot') != snapshot or source.get('commit') != original['commit']:
            raise ValueError('Packaged source provenance does not match its archive')
    return manifest


def package(build, output, provenance, source_archive=None, build_receipt=None):
    files = inputs(build)
    if bool(source_archive) != bool(build_receipt):
        raise ValueError('Source archive and build receipt must be supplied together')
    if source_archive:
        receipt = json.loads(build_receipt.read_text())
        snapshot, original = check_build_binding(files, source_archive, receipt)
        provenance = {'commit': original['commit'], 'dirty': original['dirty'], 'snapshot': snapshot,
                      'snapshotArchiveIncluded': True, 'archive': 'source/surge-source-' + snapshot + '.tar.gz',
                      'buildReceipt': 'build-receipt.json', 'correspondingSourceIncluded': False}
    output.mkdir(parents=True, exist_ok=True)
    stage = Path(tempfile.mkdtemp(prefix='.surge-package-', dir=output))
    try:
        for name, source in files.items():
            target = stage / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, target)
        shutil.copyfile(stage / 'surge-xt-browser.html', stage / 'index.html')
        shutil.copyfile(ROOT / 'LICENSE', stage / 'LICENSE')
        (stage / 'licenses').mkdir()
        for notice in ('Lua-LICENSE.txt', 'BitOp-LICENSE.txt'):
            shutil.copyfile(local_file(ROOT, 'cmake/vendor/' + notice), stage / 'licenses' / notice)
        if source_archive:
            (stage / 'source').mkdir()
            shutil.copyfile(source_archive, stage / provenance['archive'])
            shutil.copyfile(build_receipt, stage / 'build-receipt.json')
        (stage / '_headers').write_text('/*\n  Cross-Origin-Opener-Policy: same-origin\n  Cross-Origin-Embedder-Policy: require-corp\n  Cross-Origin-Resource-Policy: same-origin\n  Cache-Control: public, max-age=31536000, immutable\n')
        (stage / 'DISTRIBUTION.md').write_text(
            '# Surge XT browser development distribution\n\n'
            'This is not a completed browser release or a portable-feature parity approval.\n'
            'Dependency notices and corresponding-source release packaging remain outstanding.\n'
            'The root Surge licence is included as LICENSE; it is not a complete dependency inventory.\n\n'
            'Lua and BitOp MIT notices are included in licenses/. Other dependency notices still need audit.\n\n'
            + ('The source/ archive and build-receipt.json identify the checked snapshot and built file hashes.\n'
               'This unsigned receipt records the local build workflow, not independent build attestation.\n\n' if source_archive else '') +
            'Host this entire version directory over HTTPS and open index.html. Keep its URL immutable.\n'
            'Every response needs Cross-Origin-Opener-Policy: same-origin,\n'
            'Cross-Origin-Embedder-Policy: require-corp, and Cross-Origin-Resource-Policy: same-origin.\n'
            '_headers is an example for static hosts supporting that format; configure equivalent\n'
            'response headers on other hosts. Serve .wasm as application/wasm and .js as text/javascript.\n'
            'Keep old version directories available while their pages remain open.\n'
            'Do not apply immutable caching to a mutable latest-version redirect or landing page.\n')
        manifest = {'schema': 1, 'status': 'development', 'permissions': 'files:0644,directories:0755', 'source': provenance, 'files': {}}
        for path in sorted(stage.rglob('*')):
            if path.is_file():
                manifest['files'][path.relative_to(stage).as_posix()] = {'size': path.stat().st_size, 'sha256': digest(path)}
        raw = (json.dumps(manifest, sort_keys=True, indent=2) + '\n').encode()
        (stage / 'distribution.json').write_bytes(raw)
        for path in [stage, *stage.rglob('*')]:
            path.chmod(0o755 if path.is_dir() else 0o644)
        target = output / hashlib.sha256(raw).hexdigest()
        if target.exists():
            verify(target)
            return target
        # Recheck manifest-referenced objects after copying, then validate the
        # entire staged directory before its atomic publication.
        inputs(stage)
        verify(stage, target.name)
        os.rename(stage, target)
        stage = None
        return target
    finally:
        if stage is not None:
            shutil.rmtree(stage)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, default=ROOT / 'build-web/web')
    parser.add_argument('--output', type=Path, default=ROOT / 'web/dist')
    parser.add_argument('--verify', type=Path)
    parser.add_argument('--source-archive', type=Path)
    parser.add_argument('--build-receipt', type=Path)
    args = parser.parse_args()
    if args.verify:
        result = verify(args.verify.resolve())
        print(f'Verified {len(result["files"])} files in {args.verify}')
        return
    snapshot = ROOT / 'SOURCE-DISTRIBUTION.json'
    if not (ROOT / '.git').exists() and snapshot.is_file():
        source = json.loads(snapshot.read_text())
        # The snapshot identifies the starting sources; without Git, later
        # local edits are unknown rather than falsely reported as clean.
        provenance = {'commit': source['commit'], 'snapshot': digest(snapshot), 'dirty': None,
                      'correspondingSourceIncluded': False}
    else:
        head = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
        dirty = bool(subprocess.check_output(['git', 'status', '--porcelain', '--untracked-files=normal'], cwd=ROOT))
        provenance = {'commit': head, 'dirty': dirty, 'correspondingSourceIncluded': False}
    target = package(args.build_dir.resolve(), args.output.resolve(), provenance,
                     args.source_archive.resolve() if args.source_archive else None,
                     args.build_receipt.resolve() if args.build_receipt else None)
    print(target)


if __name__ == '__main__':
    main()
