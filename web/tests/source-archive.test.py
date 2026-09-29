import hashlib
import importlib.util
from pathlib import Path
import subprocess
import tarfile
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('source_archive', Path(__file__).parents[1] / 'scripts/source-archive.py')
source_archive = importlib.util.module_from_spec(spec)
spec.loader.exec_module(source_archive)


class SourceArchiveTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve() / 'repo'
        self.root.mkdir()
        self.output = self.root.parent / 'out'
        def git(*args):
            return subprocess.check_output(['git', '-C', str(self.root), *args], stderr=subprocess.DEVNULL)
        self.git = git
        git('init'); git('config', 'user.email', 'source-test@example.invalid'); git('config', 'user.name', 'Source test')
        (self.root / 'tracked.txt').write_text('original')
        (self.root / '.gitignore').write_text('web/cache/\n')
        git('add', '.'); git('commit', '-m', 'test: create source fixture')
        (self.root / 'tracked.txt').write_text('modified')
        (self.root / 'web').mkdir()
        (self.root / 'web/new.js').write_text('new implementation')
        (self.root / 'web/cache').mkdir()
        (self.root / 'web/cache/secret.txt').write_text('ignored')
        (self.root / 'unrelated.txt').write_text('not part of migration')

    def test_worktree_and_migration_additions_are_reproducible(self):
        first = source_archive.export(self.root, self.output)
        data = first.read_bytes()
        version, manifest = source_archive.verify(first)
        self.assertEqual(manifest['untrackedIncluded'], ['web/new.js'])
        self.assertTrue(manifest['dirty'])
        self.assertNotIn('unrelated.txt', manifest['files'])
        self.assertNotIn('web/cache/secret.txt', manifest['files'])
        self.assertEqual(manifest['files']['tracked.txt']['sha256'], hashlib.sha256(b'modified').hexdigest())
        with tarfile.open(first) as archive:
            self.assertEqual(archive.extractfile('surge-source/web/new.js').read(), b'new implementation')
            self.assertIn(manifest['commit'][:7].encode(), archive.extractfile('surge-source/VERSION_GIT_INFO').read())
        first.unlink()
        second = source_archive.export(self.root, self.output)
        self.assertEqual(second.read_bytes(), data)
        self.assertIn(version, second.name)

    def test_modified_source_gets_another_archive(self):
        first = source_archive.export(self.root, self.output)
        (self.root / 'web/new.js').write_text('another implementation')
        second = source_archive.export(self.root, self.output)
        self.assertNotEqual(first, second)
        source_archive.verify(first); source_archive.verify(second)

    def test_unreviewed_untracked_file_type_is_rejected(self):
        (self.root / 'web/credentials.env').write_text('do not include')
        with self.assertRaisesRegex(ValueError, 'Review untracked'):
            source_archive.export(self.root, self.output)
        self.assertFalse(self.output.exists())

    def test_reviewed_dependency_archives_are_embedded_exactly(self):
        vendor = self.root / 'cmake/vendor'
        vendor.mkdir(parents=True)
        for name in source_archive.BUNDLED_ARCHIVES:
            (self.root / name).write_bytes((source_archive.ROOT / name).read_bytes())
        archive = source_archive.export(self.root, self.output)
        _, manifest = source_archive.verify(archive)
        with tarfile.open(archive) as tar:
            for name in source_archive.BUNDLED_ARCHIVES:
                data = (self.root / name).read_bytes()
                self.assertEqual(tar.extractfile('surge-source/' + name).read(), data)
                self.assertEqual(manifest['files'][name]['sha256'], hashlib.sha256(data).hexdigest())

    def test_unreviewed_dependency_archive_is_rejected(self):
        vendor = self.root / 'cmake/vendor'
        vendor.mkdir(parents=True)
        (vendor / 'unknown.tar.gz').write_bytes(b'unreviewed')
        with self.assertRaisesRegex(ValueError, 'Review untracked'):
            source_archive.export(self.root, self.output)

    def extracted(self):
        archive = source_archive.export(self.root, self.output)
        digest, manifest = source_archive.verify(archive)
        directory = self.root.parent / 'extracted'
        with tarfile.open(archive) as tar:
            tar.extractall(directory, filter='data')
        return directory / 'surge-source', manifest, digest

    def test_extracted_tree_allows_build_outputs_but_rejects_changed_sources(self):
        root, manifest, digest = self.extracted()
        source_archive.verify_tree(root, manifest, digest)
        (root / 'build-web').mkdir()
        (root / 'build-web/app.wasm').write_bytes(b'generated binary')
        source_archive.verify_tree(root, manifest, digest)
        (root / 'web/new.js').write_text('changed implementation')
        with self.assertRaisesRegex(ValueError, 'Changed source content'):
            source_archive.verify_tree(root, manifest, digest)

    def test_extracted_tree_rejects_new_sources_and_changed_manifest(self):
        root, manifest, digest = self.extracted()
        extra = root / 'web/extra.js'
        extra.write_text('unrecorded implementation')
        with self.assertRaisesRegex(ValueError, 'missing or unlisted'):
            source_archive.verify_tree(root, manifest, digest)
        extra.unlink()
        (root / 'SOURCE-DISTRIBUTION.json').write_text('{}')
        with self.assertRaisesRegex(ValueError, 'Changed extracted source manifest'):
            source_archive.verify_tree(root, manifest, digest)

    def test_extracted_tree_rejects_permissions_and_source_symlink_substitution(self):
        root, manifest, digest = self.extracted()
        path = root / 'web/new.js'
        path.chmod(0o755)
        with self.assertRaisesRegex(ValueError, 'permissions'):
            source_archive.verify_tree(root, manifest, digest)
        path.unlink()
        path.symlink_to('../tracked.txt')
        with self.assertRaisesRegex(ValueError, 'type or permissions'):
            source_archive.verify_tree(root, manifest, digest)

    def test_extracted_tree_rejects_build_directory_redirection(self):
        root, manifest, digest = self.extracted()
        (root / 'build-web').symlink_to(self.root, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, 'build directory must not be a symlink'):
            source_archive.verify_tree(root, manifest, digest)

    def test_submodule_worktree_is_embedded_without_its_untracked_cache(self):
        dependency = self.root.parent / 'dependency'
        dependency.mkdir()
        def git(*args):
            return subprocess.check_output(['git', '-C', str(dependency), *args], stderr=subprocess.DEVNULL)
        git('init'); git('config', 'user.email', 'source-test@example.invalid'); git('config', 'user.name', 'Source test')
        (dependency / 'dsp.c').write_text('original DSP')
        git('add', '.'); git('commit', '-m', 'test: create dependency fixture')
        self.git('-c', 'protocol.file.allow=always', 'submodule', 'add', str(dependency), 'libs/dependency')
        self.git('commit', '-m', 'test: record dependency fixture')
        (self.root / 'libs/dependency/dsp.c').write_text('modified DSP')
        (self.root / 'libs/dependency/cache.txt').write_text('not source')
        archive = source_archive.export(self.root, self.output)
        _, manifest = source_archive.verify(archive)
        self.assertEqual(len(manifest['submoduleStatus']), 1)
        self.assertNotIn('libs/dependency/cache.txt', manifest['files'])
        with tarfile.open(archive) as tar:
            self.assertEqual(tar.extractfile('surge-source/libs/dependency/dsp.c').read(), b'modified DSP')
        self.git('submodule', 'deinit', '--force', 'libs/dependency')
        with self.assertRaisesRegex(ValueError, 'Initialize all submodules'):
            source_archive.export(self.root, self.output)

    def test_tracked_internal_symlink_and_deleted_file_are_recorded(self):
        (self.root / 'alias').symlink_to('tracked.txt')
        (self.root / 'deleted.txt').write_text('will be deleted')
        self.git('add', 'alias', 'deleted.txt'); self.git('commit', '-m', 'test: add source link and deletion fixture')
        (self.root / 'deleted.txt').unlink()
        archive = source_archive.export(self.root, self.output)
        _, manifest = source_archive.verify(archive)
        self.assertEqual(manifest['files']['alias']['target'], 'tracked.txt')
        self.assertEqual(manifest['deletedTracked'], ['deleted.txt'])


if __name__ == '__main__':
    unittest.main()
