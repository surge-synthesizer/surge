import copy
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('license_notices', ROOT / 'web/scripts/license-notices.py')
notices = importlib.util.module_from_spec(spec)
spec.loader.exec_module(notices)

class NoticeCollectionTests(unittest.TestCase):
    def setUp(self):
        self.inventory = json.loads((ROOT / 'web/licenses/inventory.json').read_text())

    def test_preserves_every_reviewed_document_verbatim(self):
        output = notices.render()
        self.assertIn('PARTIAL AUDIT', output)
        for item in self.inventory['notices']:
            data = (ROOT / item['source']).read_bytes()
            selections = item.get('extracts', [item['extract']] if 'extract' in item else [])
            if selections:
                for select in selections:
                    text = data[select['startByte']:select['endByte']].decode(select.get('encoding', 'utf-8'))
                    self.assertIn(text, output)
            else:
                self.assertIn(data.decode('utf-8'), output)
            self.assertIn('Source: ' + item['source'], output)
        for remaining in self.inventory['remaining']:
            self.assertIn(remaining, output)

    def test_rejects_changed_sources_and_invalid_extractions(self):
        for change in ['source digest', 'range', 'excerpt digest', 'escape', 'duplicate']:
            with self.subTest(change=change):
                inventory = copy.deepcopy(self.inventory)
                item = next(n for n in inventory['notices'] if 'extract' in n)
                if change == 'source digest': item['sha256'] = '0' * 64
                if change == 'range': item['extract']['endByte'] = item['bytes'] + 1
                if change == 'excerpt digest': item['extract']['sha256'] = '0' * 64
                if change == 'escape': item['source'] = '../outside-notice.txt'
                if change == 'duplicate': inventory['notices'].append(copy.deepcopy(item))
                with self.assertRaises(ValueError): notices.render(inventory=inventory)

    def test_failed_audit_preserves_existing_bundle(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / 'notices.txt'
            output.write_text('existing reviewed bundle')
            with patch.object(sys, 'argv', ['license-notices.py', str(output)]), \
                 patch.object(notices, 'render', side_effect=ValueError('Changed upstream notice')):
                with self.assertRaises(ValueError): notices.main()
            self.assertEqual(output.read_text(), 'existing reviewed bundle')
            self.assertEqual(list(Path(directory).iterdir()), [output])

if __name__ == '__main__':
    unittest.main()
