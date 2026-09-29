#!/usr/bin/env python3
"""Publish every factory resource as a content-addressed, independently fetchable asset."""
import argparse
import gzip
import struct
import hashlib
import json
from pathlib import Path


def publish(source: Path, destination: Path):
    entries = []
    patch_index = {}
    objects = destination / 'objects'
    objects.mkdir(parents=True, exist_ok=True)
    for file in sorted(source.rglob('*')):
        if not file.is_file() or file.name == '.DS_Store':
            continue
        relative = file.relative_to(source).as_posix()
        data = file.read_bytes()
        if file.suffix.lower() == '.fxp':
            if len(data) < 92 or data[:4] != b'CcnK' or data[60:64] != b'sub3':
                raise ValueError(f'Invalid factory patch header: {relative}')
            xml_size = struct.unpack_from('<I', data, 64)[0]
            if 92 + xml_size > len(data):
                raise ValueError(f'Truncated factory patch: {relative}')
            patch_index[relative] = data[92:92 + xml_size].decode('utf-8').rstrip('\0')
        if file.suffix.lower() == '.wtscript':
            patch_index[relative] = data.decode('utf-8')
        if file.suffix.lower() in ('.srgfx', '.modpreset'):
            patch_index[relative] = data.decode('utf-8')
        if relative.startswith('skins/') and file.name == 'skin.xml':
            patch_index[relative] = data.decode('utf-8')
        digest = hashlib.sha256(data).hexdigest()
        output = objects / digest
        if not output.exists() or output.stat().st_size != len(data):
            temporary = output.with_suffix('.tmp')
            temporary.write_bytes(data)
            temporary.replace(output)
        entries.append({'path': relative, 'name': file.stem, 'category': file.relative_to(source).parent.as_posix(),
                        'extension': file.suffix.lower(), 'size': len(data), 'sha256': digest,
                        'url': 'objects/' + digest})
    catalog_bytes = gzip.compress(json.dumps(patch_index, ensure_ascii=False, separators=(',', ':')).encode(), mtime=0)
    catalog_hash = hashlib.sha256(catalog_bytes).hexdigest()
    catalog_file = objects / catalog_hash
    if not catalog_file.exists():
        catalog_file.write_bytes(catalog_bytes)
    catalog = {'path': '.patch-index.json.gz', 'size': len(catalog_bytes),
               'sha256': catalog_hash, 'url': 'objects/' + catalog_hash}
    identity = json.dumps(entries, ensure_ascii=False, separators=(',', ':')).encode()
    manifest = {'schema': 1, 'version': hashlib.sha256(identity).hexdigest(),
                'totalBytes': sum(entry['size'] for entry in entries), 'entries': entries, 'patchIndex': catalog}
    encoded = json.dumps(manifest, ensure_ascii=False, separators=(',', ':')).encode() + b'\n'
    target = destination / 'manifest.json'
    if not target.exists() or target.read_bytes() != encoded:
        temporary = target.with_suffix('.tmp')
        temporary.write_bytes(encoded)
        temporary.replace(target)
    print(f"Factory library: {len(entries)} files, {manifest['totalBytes']} bytes, {manifest['version'][:12]}")


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('destination', type=Path)
    options = parser.parse_args()
    publish(options.source, options.destination)
