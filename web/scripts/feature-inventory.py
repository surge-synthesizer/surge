#!/usr/bin/env python3
"""Index desktop UI entry points without treating compilation as browser parity."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / 'web/parity/source-inventory.json'
REVIEWS = ROOT / 'web/parity/reviews.json'


def uncomment(text):
    tokens = re.compile(r'"(?:\\.|[^"\\])*"|//[^\n]*|/\*[\s\S]*?\*/')
    return tokens.sub(lambda m: m[0] if m[0].startswith('"') else re.sub(r'[^\n]', ' ', m[0]), text)


def arguments(text, start, count):
    """Read top-level arguments, preserving nested calls and quoted commas."""
    result, begin, depth, i = [], start, 0, start
    while i < len(text):
        char = text[i]
        if char in ('"', "'"):
            quote = char
            i += 1
            while i < len(text):
                if text[i] == '\\': i += 2; continue
                if text[i] == quote: break
                i += 1
        elif char == '[' and depth == 0 and not text[begin:i].strip():
            result.append('<callback>'); break
        elif char in '([{': depth += 1
        elif char in ')]}':
            if depth == 0: result.append(text[begin:i].strip()); break
            depth -= 1
        elif char == ',' and depth == 0:
            result.append(text[begin:i].strip())
            if len(result) == count: break
            begin = i + 1
        i += 1
    return [' '.join(value.split()) for value in result]


def collect():
    entries, sources, occurrences = [], {}, {}
    def add(path, kind, name, offset, expression=None):
        relative = path.relative_to(ROOT).as_posix()
        raw = path.read_text()
        sources[relative] = hashlib.sha256(raw.encode()).hexdigest()
        base = f'{relative}:{kind}:{name}'
        occurrence = occurrences.get(base, 0) + 1
        occurrences[base] = occurrence
        entry = {'id': base + (f':{occurrence}' if occurrence > 1 else ''),
                 'kind': kind, 'name': name, 'source': relative,
                 'line': raw.count('\n', 0, offset) + 1}
        if expression is not None:
            entry['arguments'] = expression
            labels = re.findall(r'"(?:\\.|[^"\\])*"', ' '.join(expression))
            entry['labelHint'] = ' '.join(labels) if labels else 'dynamic: ' + (expression[0] if expression else '?')
            # A quoted suffix in a generated name is not a fixed menu label.
            # Only a complete literal (optionally case-adjusted) is static.
            literal = r'"(?:\\.|[^"\\])*"'
            first = expression[0] if expression else ''
            entry['requiresRuntimeExpansion'] = not bool(
                re.fullmatch(literal, first) or
                re.fullmatch(r'(?:\w+::)*toOSCase\(\s*' + literal + r'\s*\)', first))
        entries.append(entry)
    gui = ROOT / 'src/surge-xt/gui'
    paths = sorted([*gui.rglob('*.cpp'), *gui.rglob('*.h'), ROOT / 'src/surge-xt/SurgeSynthEditor.cpp',
                    ROOT / 'libs/JUCE/modules/juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h',
                    ROOT / 'libs/JUCE/modules/juce_audio_utils/gui/juce_AudioDeviceSelectorComponent.cpp'])
    calls = re.compile(r'(?:\.|->)\s*(addItem|addSubMenu)\s*\(|\b(addMenuItemWithShortcut)\s*\(')
    for path in paths:
        text = uncomment(path.read_text())
        for match in calls.finditer(text):
            args = arguments(text, match.end(), 3)
            signature = '|'.join(args)
            identifier = hashlib.sha256(signature.encode()).hexdigest()[:16]
            add(path, 'menu-call', identifier, match.start(), args)
    enums = [('src/surge-xt/gui/SurgeGUIEditorKeyboardActions.h', 'KeyboardActions', 'shortcut'),
             ('src/common/SkinModel.h', 'NonParameterConnection', 'skin-action'),
             ('src/surge-xt/gui/SurgeGUIEditor.h', 'OverlayTags', 'overlay')]
    for relative, enum, kind in enums:
        path = ROOT / relative
        text = uncomment(path.read_text())
        match = re.search(r'\benum\s+' + enum + r'\s*\{([^}]+)\}', text)
        if not match: raise ValueError(f'Missing enum {enum}')
        for member in re.finditer(r'^\s*([A-Z][A-Z0-9_]*)\s*(?:=[^,]+)?\s*,?\s*$', match[1], re.M):
            if member[1] in ('N_NONCONNECTED', 'PARAMETER_CONNECTED', 'NO_EDITOR'): continue
            add(path, kind, member[1], match.start(1) + member.start(1))
    path = ROOT / 'src/common/SkinModel.cpp'
    text = uncomment(path.read_text())
    for match in re.finditer(r'\bConnector\s*\(\s*"([^"\n]+)"', text):
        add(path, 'skin-connector', match[1], match.start())
    for path in sorted((gui / 'overlays').glob('*.cpp')):
        add(path, 'editor-implementation', path.stem, 0)
    return {'schema': 1, 'scope': 'Static desktop UI entry points; dynamic menus and runtime parameter families still require expansion and workflow review.',
            'sources': dict(sorted(sources.items())), 'entries': sorted(entries, key=lambda entry: entry['id'])}


def audit(inventory, require_complete, review_path):
    reviews = json.loads(review_path.read_text())
    ids = {entry['id']: entry for entry in inventory['entries']}
    errors = []
    for identifier, review in reviews.items():
        entry = ids.get(identifier)
        if not entry: errors.append(f'Unknown review: {identifier}'); continue
        if review.get('sourceDigest') != inventory['sources'][entry['source']]:
            errors.append(f'Stale review: {identifier}')
        if review.get('status') == 'verified':
            evidence = review.get('evidence', [])
            if not review.get('browserEquivalent') or not evidence or any(not (ROOT / item).is_file() for item in evidence):
                errors.append(f'Missing verification evidence: {identifier}')
            if entry.get('requiresRuntimeExpansion'):
                expansion = review.get('runtimeExpansion')
                if not isinstance(expansion, list) or not expansion or any(
                        not isinstance(item, str) or not item.strip() for item in expansion):
                    errors.append(f'Missing runtime menu expansion: {identifier}')
        elif review.get('status') == 'platform-limitation':
            if review.get('capability') not in {'os-shell', 'native-driver', 'plugin-host', 'mts-esp', 'udp-osc'} or not review.get('reason') or not review.get('portableAlternative'):
                errors.append(f'Missing platform justification/alternative: {identifier}')
        else: errors.append(f'Invalid review status: {identifier}')
    pending = len(ids.keys() - reviews.keys())
    if require_complete and pending: errors.append(f'{pending} entry points have no browser parity review')
    print(f'{len(ids)} entry points; {len(reviews)} reviewed; {pending} unreviewed')
    for error in errors: print(error, file=sys.stderr)
    return not errors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--write', action='store_true')
    parser.add_argument('--require-complete', action='store_true')
    parser.add_argument('--reviews', type=Path, default=REVIEWS)
    args = parser.parse_args()
    inventory = collect()
    content = json.dumps(inventory, indent=2, ensure_ascii=False) + '\n'
    if args.write: OUTPUT.write_text(content)
    elif not OUTPUT.exists() or OUTPUT.read_text() != content:
        sys.exit('UI inventory changed; regenerate with --write and review the changes')
    if not audit(inventory, args.require_complete, args.reviews): sys.exit(1)

if __name__ == '__main__': main()
