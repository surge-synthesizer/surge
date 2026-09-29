#!/usr/bin/env python3
"""Stage or build the static browser deployment without uploading the factory library."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import shutil
import subprocess
import tarfile
import urllib.request

ROOT = Path(__file__).resolve().parents[2]

def checked(data, item):
    if len(data) != item['size'] or hashlib.sha256(data).hexdigest() != item['sha256']:
        raise ValueError('Asset integrity failure: ' + item['path'])
    return data

def restore(stage):
    config = json.loads((stage / 'factory-source.json').read_text())
    commit = config['commit']
    if not re.fullmatch('[0-9a-f]{40}', commit):
        raise ValueError('Expected a pinned factory source commit')
    public = stage / 'public'
    manifest = json.loads((public / 'library/manifest.json').read_text())
    entries = {x['path']: x for x in manifest['entries']}
    prefix = 'surge-' + commit + '/resources/data/'
    found = set()
    # Stream the archive; do not extract paths supplied by the archive.
    with urllib.request.urlopen('https://codeload.github.com/surge-synthesizer/surge/tar.gz/' + commit, timeout=120) as response:
        with tarfile.open(fileobj=response, mode='r|gz') as archive:
            for member in archive:
                if not member.name.startswith(prefix):
                    continue
                name = member.name[len(prefix):]
                if name not in entries or not member.isfile():
                    continue
                item = entries[name]
                data = checked(archive.extractfile(member).read(), item)
                (public / 'library/objects' / item['sha256']).write_bytes(data)
                found.add(name)
    if found != set(entries):
        raise ValueError('Factory archive missing entries: ' + repr(sorted(set(entries) - found)))
    for item in [*manifest['entries'], manifest['patchIndex']]:
        checked((public / 'library/objects' / item['sha256']).read_bytes(), item)
    print(f'Verified {len(entries)} factory assets and the patch index')

def stage_package(package, destination, commit):
    spec = importlib.util.spec_from_file_location('package_static', ROOT / 'web/scripts/package-static.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    distribution = module.verify(package)
    destination.mkdir(parents=True, exist_ok=False)
    public = destination / 'public'
    public.mkdir()
    manifest = json.loads((package / 'library/manifest.json').read_text())
    index = 'library/' + manifest['patchIndex']['url']
    for name in distribution['files']:
        if name.startswith('library/objects/') and name != index:
            continue
        target = public / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(package / name, target)
    shutil.copyfile(package / 'distribution.json', public / 'distribution.json')
    shutil.copyfile(__file__, destination / 'build.py')
    (destination / '.vercelignore').write_text(
        'public/library/objects/*\n!public/' + index + '\n.env*\n')
    source = public / 'source'
    source.mkdir()
    # Reconstruct from the pinned upstream commit, including migration changes
    # that have already been committed on this branch.
    changed = subprocess.check_output(['git', 'diff', '--name-only', commit, '-z'], cwd=ROOT).decode().split('\0')
    added = subprocess.check_output(['git', 'ls-files', '--others', '--exclude-standard', '-z'], cwd=ROOT).decode().split('\0')
    names = {name for name in changed + added if name and (ROOT / name).is_file()
             and (name in changed or name.split('/')[0] in ('web', 'src', 'cmake'))}
    with tarfile.open(source / 'surge-browser-changes.tar.gz', 'w:gz') as archive:
        for name in sorted(names):
            archive.add(ROOT / name, arcname=name, recursive=False)
    (source / 'README.txt').write_text(
        'Reconstruct the Surge XT browser development source:\n\n'
        'git clone https://github.com/surge-synthesizer/surge.git\ncd surge\n'
        'git checkout ' + commit + '\n'
        'git submodule update --init --recursive\n'
        'tar -xzf /path/to/surge-browser-changes.tar.gz\n\n'
        'The archive overlays the modified tracked files and migration additions.\n'
        'See web/README.md and web/scripts/build.sh for pinned-toolchain build instructions.\n'
        'Factory resources are verified against /library/manifest.json during deployment.\n'
        'This is an unfinished development port, not a parity-certified release.\n')
    notices_spec = importlib.util.spec_from_file_location('license_notices', ROOT / 'web/scripts/license-notices.py')
    notices = importlib.util.module_from_spec(notices_spec)
    notices_spec.loader.exec_module(notices)
    (public / 'THIRD-PARTY-NOTICES.txt').write_text(notices.render())
    (destination / 'factory-source.json').write_text(json.dumps({'commit': commit}) + '\n')
    (destination / 'vercel.json').write_text(json.dumps({
        '$schema': 'https://openapi.vercel.sh/vercel.json',
        'framework': None, 'installCommand': '', 'buildCommand': 'python3 build.py --restore .',
        'outputDirectory': 'public',
        'headers': [
            {'source': '/(.*)', 'headers': [
                {'key': 'Cross-Origin-Opener-Policy', 'value': 'same-origin'},
                {'key': 'Cross-Origin-Embedder-Policy', 'value': 'require-corp'},
                {'key': 'Cross-Origin-Resource-Policy', 'value': 'same-origin'},
                {'key': 'Cache-Control', 'value': 'public, max-age=0, must-revalidate'}]},
            {'source': '/library/objects/(.*)', 'headers': [
                {'key': 'Cache-Control', 'value': 'public, max-age=31536000, immutable'}]},
            {'source': '/(.*).wasm', 'headers': [
                {'key': 'Content-Type', 'value': 'application/wasm'}]}
        ]}, indent=2) + '\n')
    print(destination)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--restore', type=Path)
    parser.add_argument('--package', type=Path)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--commit')
    args = parser.parse_args()
    if args.restore:
        restore(args.restore.resolve())
    elif args.package and args.output and args.commit:
        stage_package(args.package.resolve(), args.output.resolve(), args.commit)
    else:
        parser.error('Use --restore DIR or --package DIR --output DIR --commit SHA')
