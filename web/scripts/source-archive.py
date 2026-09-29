#!/usr/bin/env python3
"""Snapshot current build sources, including migration additions and submodules."""
import argparse
import gzip
import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import subprocess
import tarfile
import tempfile

ROOT = Path(__file__).resolve().parents[2]
PREFIX = 'surge-source/'
EXTENSIONS = {'.cpp', '.h', '.hpp', '.cmake', '.js', '.html', '.json', '.py', '.mjs', '.md', '.txt', '.sh', '.c', '.inc'}
BUNDLED_ARCHIVES = {
    'cmake/vendor/lua-5.1.5.tar.gz',
    'cmake/vendor/luabitop-81bb23b0e737805442033535de8e6d204d0e5381.tar.gz',
}


def git(root, *args):
    return subprocess.check_output(['git', '-C', str(root), *args])


def sha_stream(stream):
    h = hashlib.sha256()
    for block in iter(lambda: stream.read(1024 * 1024), b''):
        h.update(block)
    return h.hexdigest()


def safe_name(name):
    p = PurePosixPath(name)
    if not name or p.is_absolute() or '..' in p.parts or '\\' in name:
        raise ValueError('Unsafe source path: ' + name)
    return p


def snapshot(root):
    status = git(root, 'submodule', 'status', '--recursive').decode()
    if any(line.startswith('-') for line in status.splitlines()):
        raise ValueError('Initialize all submodules before exporting source')
    selected = {}
    for row in git(root, 'ls-files', '--recurse-submodules', '--stage', '-z').decode().split('\0'):
        if not row:
            continue
        meta, name = row.split('\t', 1)
        mode, _, stage = meta.split()
        if stage != '0' or mode not in ('100644', '100755', '120000'):
            raise ValueError('Unresolved source index entry: ' + name)
        selected[name] = mode
    additions = []
    for name in git(root, 'ls-files', '--others', '--exclude-standard', '-z').decode().split('\0'):
        if not name or PurePosixPath(name).parts[0] not in ('web', 'src', 'cmake'):
            continue
        if (Path(name).suffix not in EXTENSIONS and name not in BUNDLED_ARCHIVES) or any(part.startswith('.') for part in PurePosixPath(name).parts):
            raise ValueError('Review untracked migration file before source export: ' + name)
        selected[name] = '100755' if (root / name).stat().st_mode & 0o111 else '100644'
        additions.append(name)
    files, sources, deleted = {}, {}, []
    for name, mode in sorted(selected.items()):
        safe_name(name)
        path = root / name
        if not path.exists() and not path.is_symlink():
            deleted.append(name)
            continue
        if mode == '120000':
            target = os.readlink(path)
            safe_name(target)
            files[name] = {'kind': 'symlink', 'target': target, 'mode': 0o777}
        else:
            if path.is_symlink() or not path.is_file():
                raise ValueError('Source file changed type: ' + name)
            with path.open('rb') as source:
                digest = sha_stream(source)
            files[name] = {'kind': 'file', 'mode': int(mode[-3:], 8), 'size': path.stat().st_size, 'sha256': digest}
            sources[name] = path
    commit = git(root, 'rev-parse', 'HEAD').decode().strip()
    branch = git(root, 'rev-parse', '--abbrev-ref', 'HEAD').decode().strip()
    short = git(root, 'rev-parse', '--short', 'HEAD').decode().strip()
    version = f'Surge source snapshot\n{branch}\n{short}\n'.encode()
    name = 'VERSION_GIT_INFO'
    files[name] = {'kind': 'file', 'mode': 0o644, 'size': len(version), 'sha256': hashlib.sha256(version).hexdigest()}
    sources[name] = version
    manifest = {'schema': 1, 'scope': 'Current tracked worktree and embedded submodules plus untracked migration source files, including bundled Lua/BitOp source archives; the toolchain is obtained by the pinned build script.',
                'commit': commit, 'dirty': bool(git(root, 'status', '--porcelain')),
                'submoduleStatus': status.splitlines(), 'untrackedIncluded': sorted(additions),
                'deletedTracked': deleted, 'files': files}
    return (json.dumps(manifest, sort_keys=True, indent=2) + '\n').encode(), sources


def verify(archive):
    with tarfile.open(archive, 'r|gz') as tar:
        first = next(iter(tar), None)
        if first is None or first.name != PREFIX + 'SOURCE-DISTRIBUTION.json' or not first.isfile():
            raise ValueError('Missing source distribution manifest')
        raw = tar.extractfile(first).read()
        manifest = json.loads(raw)
        if manifest.get('schema') != 1:
            raise ValueError('Unsupported source manifest')
        expected = manifest['files']
        seen = set()
        for member in tar:
            if member is first:
                continue
            if not member.name.startswith(PREFIX):
                raise ValueError('Source entry outside archive root')
            name = member.name[len(PREFIX):]
            safe_name(name)
            if name in seen or name not in expected:
                raise ValueError('Unlisted or repeated source entry: ' + name)
            seen.add(name)
            info = expected[name]
            if member.mode != info['mode'] or member.uid or member.gid or member.mtime:
                raise ValueError('Noncanonical source metadata: ' + name)
            if info['kind'] == 'symlink':
                safe_name(member.linkname)
                if not member.issym() or member.linkname != info['target']:
                    raise ValueError('Changed source symlink: ' + name)
            elif not member.isfile() or member.size != info['size'] or sha_stream(tar.extractfile(member)) != info['sha256']:
                raise ValueError('Source integrity failure: ' + name)
        if seen != set(expected):
            raise ValueError('Source archive is incomplete')
    return hashlib.sha256(raw).hexdigest(), manifest


def export(root, output):
    raw, sources = snapshot(root)
    manifest = json.loads(raw)
    output.mkdir(parents=True, exist_ok=True)
    target = output / ('surge-source-' + hashlib.sha256(raw).hexdigest() + '.tar.gz')
    if target.exists():
        digest, _ = verify(target)
        if digest != hashlib.sha256(raw).hexdigest():
            raise ValueError('Existing source archive has another manifest')
        return target
    handle, temporary = tempfile.mkstemp(prefix='.surge-source-', suffix='.tar.gz', dir=output)
    try:
        with os.fdopen(handle, 'wb') as stream, gzip.GzipFile(filename='', mode='wb', fileobj=stream, mtime=0, compresslevel=6) as zipped, tarfile.open(fileobj=zipped, mode='w|', format=tarfile.PAX_FORMAT) as tar:
            info = tarfile.TarInfo(PREFIX + 'SOURCE-DISTRIBUTION.json')
            info.size = len(raw); info.mode = 0o644
            tar.addfile(info, io.BytesIO(raw))
            for name, record in manifest['files'].items():
                info = tarfile.TarInfo(PREFIX + name)
                info.mode = record['mode']
                if record['kind'] == 'symlink':
                    info.type = tarfile.SYMTYPE; info.linkname = record['target']
                    tar.addfile(info)
                else:
                    info.size = record['size']
                    source = sources[name]
                    with io.BytesIO(source) if isinstance(source, bytes) else source.open('rb') as data:
                        tar.addfile(info, data)
        verify(temporary)
        os.chmod(temporary, 0o644)
        os.rename(temporary, target)
        return target
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def verify_tree(root, manifest, manifest_digest, build_directory='build-web'):
    """Check an extracted snapshot before/after an isolated build."""
    metadata = root / 'SOURCE-DISTRIBUTION.json'
    if metadata.is_symlink() or not metadata.is_file():
        raise ValueError('Missing extracted source manifest')
    if hashlib.sha256(metadata.read_bytes()).hexdigest() != manifest_digest:
        raise ValueError('Changed extracted source manifest')
    if (root / build_directory).is_symlink():
        raise ValueError('Extracted build directory must not be a symlink')
    expected = manifest['files']
    actual = set()
    for path in root.rglob('*'):
        name = path.relative_to(root).as_posix()
        if name == build_directory or name.startswith(build_directory + '/'):
            continue
        if path.is_file() or path.is_symlink():
            actual.add(name)
    if actual != set(expected) | {'SOURCE-DISTRIBUTION.json'}:
        raise ValueError('Extracted source tree contains missing or unlisted files')
    for name, info in expected.items():
        safe_name(name)
        path = root / name
        if any(parent.is_symlink() for parent in path.parents if parent != root.parent):
            raise ValueError('Symlink parent in extracted source: ' + name)
        if info['kind'] == 'symlink':
            if not path.is_symlink() or os.readlink(path) != info['target']:
                raise ValueError('Changed source symlink: ' + name)
        else:
            if path.is_symlink() or not path.is_file() or path.stat().st_mode & 0o777 != info['mode']:
                raise ValueError('Changed source file type or permissions: ' + name)
            with path.open('rb') as data:
                if path.stat().st_size != info['size'] or sha_stream(data) != info['sha256']:
                    raise ValueError('Changed source content: ' + name)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'web/dist')
    parser.add_argument('--verify', type=Path)
    parser.add_argument('--tree', type=Path, help='Also verify this extracted tree against --verify')
    args = parser.parse_args()
    if args.tree and not args.verify:
        parser.error('--tree requires --verify')
    if args.verify:
        version, manifest = verify(args.verify)
        if args.tree:
            verify_tree(args.tree.resolve(), manifest, version)
        print(f'Verified {len(manifest["files"])} source entries; manifest {version}')
    else:
        print(export(ROOT, args.output.resolve()))


if __name__ == '__main__':
    main()
