"""Distribution integrity and publication failure tests; no network needed."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import subprocess
import tarfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('packaging', Path(__file__).parents[1] / 'scripts/package-static.py')
packaging = importlib.util.module_from_spec(spec)
spec.loader.exec_module(packaging)


class PackageTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name).resolve()
        self.build = self.root / 'build'
        self.build.mkdir()
        for name, content in {'surge-xt-browser.html': '<script src="surge-xt-browser.js"></script><script src="runtime.js"></script>',
                              'surge-xt-browser.js': 'wasm startup', 'runtime.js': 'runtime',
                              'surge-xt-browser.wasm': 'wasm', 'surge-xt-browser.data': 'fonts',
                              'library.js': 'catalog', 'surge-xt-browser.worker.js': 'worker',
                              'surge-juce-browser-check.js': 'do not ship', 'secret.txt': 'do not ship'}.items():
            (self.build / name).write_text(content)
        library = self.build / 'library'
        (library / 'objects').mkdir(parents=True)
        data = b'factory resource'
        sha = hashlib.sha256(data).hexdigest()
        self.object = library / 'objects' / sha
        self.object.write_bytes(data)
        entry = {'path': 'factory.fxp', 'url': 'objects/' + sha, 'size': len(data), 'sha256': sha}
        self.manifest = library / 'manifest.json'
        self.manifest.write_text(json.dumps({'schema': 1, 'entries': [entry], 'patchIndex': entry}))
        self.output = self.root / 'dist'
        self.provenance = {'commit': 'test', 'dirty': True, 'correspondingSourceIncluded': False}

    def package(self):
        return packaging.package(self.build, self.output, self.provenance)

    def test_reproducible_allowlist_and_integrity(self):
        first = self.package()
        self.assertEqual(first, self.package())
        manifest = packaging.verify(first)
        self.assertEqual(manifest['status'], 'development')
        self.assertEqual((first / 'index.html').read_bytes(), (self.build / 'surge-xt-browser.html').read_bytes())
        self.assertIn('surge-xt-browser.worker.js', manifest['files'])
        self.assertNotIn('secret.txt', manifest['files'])
        self.assertNotIn('surge-juce-browser-check.js', manifest['files'])
        for notice in ('Lua-LICENSE.txt', 'BitOp-LICENSE.txt'):
            self.assertEqual((first / 'licenses' / notice).read_bytes(),
                             (packaging.ROOT / 'cmake/vendor' / notice).read_bytes())
            self.assertIn('licenses/' + notice, manifest['files'])
        (self.build / 'runtime.js').write_text('changed runtime')
        self.assertNotEqual(first, self.package())
        self.assertTrue((first / 'runtime.js').exists())

    def test_tampering_cannot_be_silently_reused(self):
        result = self.package()
        (result / 'runtime.js').write_text('corrupt')
        with self.assertRaisesRegex(ValueError, 'integrity failure'):
            self.package()
        self.assertEqual([p.name for p in self.output.iterdir()], [result.name])

    def test_corrupt_object_is_rejected_before_publication(self):
        self.object.write_bytes(b'bad')
        with self.assertRaisesRegex(ValueError, 'does not match manifest'):
            self.package()
        self.assertFalse(self.output.exists())

    def test_source_changed_during_copy_cannot_publish(self):
        original = packaging.shutil.copyfile
        def copy(source, destination):
            result = original(source, destination)
            if Path(source) == self.object:
                Path(destination).write_bytes(b'changed during copy')
            return result
        with patch.object(packaging.shutil, 'copyfile', side_effect=copy):
            with self.assertRaisesRegex(ValueError, 'does not match manifest'):
                self.package()
        self.assertEqual(list(self.output.iterdir()), [])

    def test_symlinks_and_path_traversal_are_rejected(self):
        outside = self.root / 'outside.js'
        outside.write_text('outside')
        (self.build / 'runtime.js').unlink()
        (self.build / 'runtime.js').symlink_to(outside)
        with self.assertRaisesRegex(ValueError, 'Symlink'):
            self.package()
        (self.build / 'runtime.js').unlink()
        (self.build / 'runtime.js').write_text('runtime')
        (self.build / 'surge-xt-browser.html').write_text('<script src="surge-xt-browser.js"></script><script src="../outside.js"></script>')
        with self.assertRaisesRegex(ValueError, 'Invalid package path'):
            self.package()

    def source_binding(self):
        source = self.root / 'source-fixture'
        source.mkdir()
        def git(*args):
            return subprocess.check_output(['git', '-C', str(source), *args], stderr=subprocess.DEVNULL)
        git('init'); git('config', 'user.email', 'source-test@example.invalid'); git('config', 'user.name', 'Source test')
        (source / 'engine.c').write_text('fixture source')
        git('add', '.'); git('commit', '-m', 'test: create package source fixture')
        archive = packaging.source_tools().export(source, self.root / 'source-archives')
        snapshot, original = packaging.source_tools().verify(archive)
        receipt = {'schema': 1, 'kind': 'surge-browser-isolated-build', 'snapshot': snapshot,
                   'sourceCommit': original['commit'], 'sourceTreeVerifiedBeforeAndAfter': True,
                   'outputs': packaging.file_records(packaging.inputs(self.build))}
        receipt_path = self.root / 'build-receipt.json'
        receipt_path.write_text(json.dumps(receipt))
        return archive, receipt_path, receipt

    def test_source_archive_and_receipt_are_bound_to_packaged_outputs(self):
        archive, receipt, _ = self.source_binding()
        target = packaging.package(self.build, self.output, {}, archive, receipt)
        manifest = packaging.verify(target)
        self.assertTrue(manifest['source']['snapshotArchiveIncluded'])
        self.assertFalse(manifest['source']['correspondingSourceIncluded'])
        self.assertEqual((target / manifest['source']['archive']).read_bytes(), archive.read_bytes())
        self.assertEqual((target / 'build-receipt.json').read_bytes(), receipt.read_bytes())
        self.assertEqual(target, packaging.package(self.build, self.output, {}, archive, receipt))

    def test_mismatched_source_or_changed_binary_cannot_publish(self):
        archive, receipt_path, receipt = self.source_binding()
        receipt_path.write_text(json.dumps({**receipt, 'snapshot': '0' * 64}))
        with self.assertRaisesRegex(ValueError, 'verified source snapshot'):
            packaging.package(self.build, self.output, {}, archive, receipt_path)
        receipt_path.write_text(json.dumps(receipt))
        (self.build / 'surge-xt-browser.wasm').write_bytes(b'changed binary')
        with self.assertRaisesRegex(ValueError, 'outputs do not match'):
            packaging.package(self.build, self.output, {}, archive, receipt_path)
        self.assertFalse(self.output.exists())

    def test_archive_and_receipt_are_required_together(self):
        archive, receipt, _ = self.source_binding()
        for source, record in [(archive, None), (None, receipt)]:
            with self.assertRaisesRegex(ValueError, 'supplied together'):
                packaging.package(self.build, self.output, {}, source, record)

    def test_source_changed_during_copy_cannot_publish_bound_package(self):
        archive, receipt, _ = self.source_binding()
        copyfile = packaging.shutil.copyfile
        def copy(source, destination):
            result = copyfile(source, destination)
            if Path(source) == archive:
                Path(destination).write_bytes(b'broken archive')
            return result
        with patch.object(packaging.shutil, 'copyfile', side_effect=copy):
            with self.assertRaises(tarfile.ReadError):
                packaging.package(self.build, self.output, {}, archive, receipt)
        self.assertEqual(list(self.output.iterdir()), [])


if __name__ == '__main__':
    unittest.main()
