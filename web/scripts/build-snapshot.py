#!/usr/bin/env python3
"""Build a fresh checked source snapshot and package it with an unsigned receipt."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import platform
import re
import subprocess
import sys
import tarfile
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SDK_VERSION = '6.0.10'
SDK_COMMIT = 'a2b92777574c2feda07994cd4f1079a3dfc151f8'
CONFIGURE = ['-DENABLE_LTO=OFF', '-DSURGE_BUILD_TESTRUNNER=OFF', '-DSURGE_SKIP_WERROR=ON']
TARGETS = ['surge-xt-browser']


def module(root, name):
    spec = importlib.util.spec_from_file_location(name.replace('-', '_'), root / 'web/scripts' / (name + '.py'))
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def build(output, work, sdk, jobs):
    # Avoid adding bytecode files to the extracted sources we verify below.
    sys.dont_write_bytecode = True
    env = dict(os.environ, PYTHONDONTWRITEBYTECODE='1', EMSDK=str(sdk))
    revision = subprocess.check_output(['git', '-C', str(sdk), 'rev-parse', 'HEAD'], text=True).strip()
    if revision != SDK_COMMIT:
        raise ValueError('Install the pinned Emscripten SDK before building a snapshot')
    subprocess.run(['git', '-C', str(sdk), 'diff', '--exit-code', 'HEAD', '--'], check=True,
                   stdout=subprocess.DEVNULL)
    compiler = sdk / 'upstream/emscripten'
    banner = subprocess.check_output([str(compiler / 'emcc'), '--version'], env=env, text=True).splitlines()[0]
    if not re.search(r'\b' + re.escape(SDK_VERSION) + r'\b', banner):
        raise ValueError('The active compiler is not the pinned Emscripten version')
    sources = module(ROOT, 'source-archive')
    archive = sources.export(ROOT, output)
    snapshot, manifest = sources.verify(archive)
    work.mkdir(parents=True, exist_ok=True)
    directory = Path(tempfile.mkdtemp(prefix=snapshot[:12] + '-', dir=work)).resolve()
    print('Build directory: ' + str(directory), flush=True)
    print('Source archive: ' + str(archive), flush=True)
    with tarfile.open(archive) as source:
        source.extractall(directory, filter='data')
    extracted = directory / 'surge-source'
    sources.verify_tree(extracted, manifest, snapshot)
    configure = [str(compiler / 'emcmake'), 'cmake', '-S', '.', '-B', 'build-web', *CONFIGURE]
    subprocess.run(configure, cwd=extracted, env=env, check=True)
    command = ['cmake', '--build', 'build-web', '--target', *TARGETS, '--parallel', str(jobs)]
    subprocess.run(command, cwd=extracted, env=env, check=True)
    sources.verify_tree(extracted, manifest, snapshot)
    packaging = module(extracted, 'package-static')
    artifacts = extracted / 'build-web/web'
    receipt = {'schema': 1, 'kind': 'surge-browser-isolated-build', 'snapshot': snapshot,
               'sourceCommit': manifest['commit'], 'sourceTreeVerifiedBeforeAndAfter': True,
               'toolchain': {'emsdkCommit': revision, 'version': SDK_VERSION, 'compiler': banner},
               'host': {'system': platform.system(), 'machine': platform.machine()},
               'configure': ['emcmake', 'cmake', '-S', '.', '-B', 'build-web', *CONFIGURE],
               'build': command, 'outputs': packaging.file_records(packaging.inputs(artifacts))}
    receipt_path = extracted / 'build-web/build-receipt.json'
    receipt_path.write_text(json.dumps(receipt, sort_keys=True, indent=2) + '\n')
    target = packaging.package(artifacts, output, {}, archive, receipt_path)
    sources.verify_tree(extracted, manifest, snapshot)
    print(json.dumps({'package': str(target), 'sourceArchive': str(archive),
                      'buildReceipt': str(receipt_path), 'sourceTree': str(extracted)}), flush=True)
    return target


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'web/dist')
    parser.add_argument('--work-dir', type=Path, default=ROOT / '.toolchains/source-builds')
    parser.add_argument('--sdk', type=Path, default=ROOT / '.toolchains' / ('emsdk-' + SDK_VERSION))
    parser.add_argument('--jobs', type=int, default=4)
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error('--jobs must be positive')
    build(args.output.resolve(), args.work_dir.resolve(), args.sdk.resolve(), args.jobs)


if __name__ == '__main__':
    main()
