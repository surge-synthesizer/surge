#!/usr/bin/env python3
"""Check the ARM64 estimate and native/Wasm Tri-pole kernel without rebuilding the app."""
import argparse
import hashlib
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--filter-include', type=Path,
                        default=ROOT / 'build-web/wasm-sst-filters/include')
    parser.add_argument('--print-table', action='store_true',
                        help='Print the 512 hardware estimate constants instead of comparing kernels')
    args = parser.parse_args()
    overlay = args.filter_include.resolve()
    if not args.print_table and not (overlay / 'sst/filters/TriPoleFilter.h').is_file():
        parser.error('Configure the Wasm build first: the generated filter overlay is missing')
    emcc = ROOT / '.toolchains/emsdk-6.0.10/upstream/emscripten/em++'
    includes = ['-I' + str(ROOT / p) for p in (
        'src/common', 'libs/sst/sst-filters/include',
        'libs/sst/sst-filters/include/sst/filters',
        'libs/sst/sst-basic-blocks/include', 'libs/sst/sst-cpputils/include', 'libs/simde')]
    with tempfile.TemporaryDirectory(prefix='surge-tripole-check-') as directory:
        temp = Path(directory)
        check = temp / 'estimate-check'
        subprocess.run(['clang++', '-std=c++17', '-O2', *includes,
                        str(ROOT / 'src/surge-web/ReciprocalSquareRootCheck.cpp'),
                        '-o', str(check)], check=True)
        subprocess.run([str(check), *(['--print-table'] if args.print_table else [])], check=True)
        if args.print_table:
            return
        source = str(ROOT / 'src/surge-web/TriPoleKernelCheck.cpp')
        native, wasm = temp / 'native', temp / 'wasm.cjs'
        common = ['-std=c++20', '-O2', '-ffp-contract=off']
        subprocess.run(['clang++', *common, *includes, source, '-o', str(native)], check=True)
        subprocess.run([str(emcc), *common, '-Wno-deprecated-pragma', '-msimd128',
                        '-I' + str(overlay), *includes, source,
                        '-sENVIRONMENT=node', '-o', str(wasm)], check=True)
        captures = []
        for name, command in [('native', [str(native)]), ('wasm', ['node', str(wasm)])]:
            capture = temp / (name + '.hex')
            with capture.open('wb') as output:
                subprocess.run(command, stdout=output, check=True)
            captures.append(capture.read_bytes())
        if captures[0] != captures[1]:
            raise RuntimeError('Tri-pole native/Wasm kernel outputs differ')
        if len(captures[0].split()) != 960000:
            raise RuntimeError('Unexpected Tri-pole sample count')
        print('All 12 Tri-pole modes match bit for bit across 960000 lane samples; SHA256 ' +
              hashlib.sha256(captures[0]).hexdigest())


if __name__ == '__main__':
    main()
