#!/usr/bin/env python3
"""Instrument the formula cache and native reference harness with ThreadSanitizer.

Build surge-engine-reference with Unix Makefiles and CMAKE_EXPORT_COMPILE_COMMANDS first. The
remaining engine libraries are reused; this checks the instrumented formula
cache paths, not the entire engine for races.
"""
import argparse
import json
from pathlib import Path
import shlex
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build', type=Path, default=Path('build-reference'))
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
build = args.build.resolve()
commands = json.loads((build / 'compile_commands.json').read_text())
with tempfile.TemporaryDirectory(prefix='surge-formula-tsan-') as temporary:
    directory = Path(temporary)
    objects = []
    for source in ['src/surge-web/Reference.cpp', 'src/common/dsp/modulators/FormulaModulationHelper.cpp']:
        entry = next(e for e in commands if Path(e['file']).resolve() == root / source)
        command = entry.get('arguments') or shlex.split(entry['command'])
        output = directory / (Path(source).stem + '.o')
        command[command.index('-o') + 1] = str(output)
        command += ['-O1', '-g', '-fsanitize=thread', '-fno-omit-frame-pointer']
        subprocess.run(command, cwd=entry['directory'], check=True)
        objects.append(str(output))
    link_dir = build / 'src/surge-web'
    command = shlex.split((link_dir / 'CMakeFiles/surge-engine-reference.dir/link.txt').read_text())
    original = next(i for i, value in enumerate(command) if value.endswith('/Reference.cpp.o'))
    command[original:original + 1] = objects
    executable = directory / 'formula-contexts'
    command[command.index('-o') + 1] = str(executable)
    command += ['-fsanitize=thread']
    subprocess.run(command, cwd=link_dir, check=True)
    subprocess.run([str(executable), '--check-formula-contexts', str(root / 'resources/data')], check=True)
