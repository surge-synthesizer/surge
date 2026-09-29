"""Build the bundled scripting sources outside the checkout and reject bad input."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class PortableLuaTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='surge-portable-lua-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        shutil.copytree(ROOT / 'cmake/vendor', self.root / 'cmake/vendor')
        shutil.copy2(ROOT / 'cmake/portable-lua.cmake', self.root / 'cmake/portable-lua.cmake')
        (self.root / 'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 3.15)
project(BundledLuaCheck C)
include(cmake/portable-lua.cmake)
add_executable(lua-smoke smoke.c)
target_link_libraries(lua-smoke PRIVATE luajit-5.1 m)
if(EMSCRIPTEN)
  target_link_options(lua-smoke PRIVATE "-sENVIRONMENT=node" "-sWASM_ASYNC_COMPILATION=0")
endif()
''')
        (self.root / 'smoke.c').write_text('''#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include <stdio.h>
extern int luaopen_bit(lua_State *L);
int main(void) {
    lua_State *L = luaL_newstate();
    if (!L) return 2;
    luaL_openlibs(L);
    luaopen_bit(L); lua_setglobal(L, "bit");
    int failed = luaL_dostring(L,
        "assert(_VERSION == 'Lua 5.1'); "
        "assert(bit.bxor(0x1234, 0xffff) == 0xedcb); "
        "local f=loadstring('return value'); setfenv(f,{value=17}); assert(f()==17)");
    if (failed) fprintf(stderr, "%s\\n", lua_tostring(L, -1));
    lua_close(L);
    if (failed) return 1;
    puts("bundled Lua 5.1 and BitOp execute successfully");
    return 0;
}
''')
        self.env = dict(os.environ)
        # No reachable proxy; each case has a fresh source/build directory.
        for name in ('http_proxy', 'https_proxy', 'all_proxy', 'HTTP_PROXY', 'HTTPS_PROXY', 'ALL_PROXY'):
            self.env[name] = 'http://127.0.0.1:9'
        self.env['no_proxy'] = self.env['NO_PROXY'] = ''

    def command(self, args):
        return subprocess.run(args, cwd=self.root, env=self.env, text=True,
                              stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=90)

    def configure(self, *args):
        return self.command(['cmake', '-S', str(self.root), '-B', str(self.root / 'build'), *args])

    def build_and_run(self, wasm=False):
        args = []
        if wasm:
            sdk = Path(os.environ.get('EMSDK', ROOT / '.toolchains/emsdk-6.0.10'))
            toolchain = sdk / 'upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake'
            self.assertTrue(toolchain.is_file(), 'Install the pinned Emscripten SDK first')
            args = ['-DCMAKE_TOOLCHAIN_FILE=' + str(toolchain)]
        configured = self.configure(*args)
        self.assertEqual(configured.returncode, 0, configured.stdout)
        built = self.command(['cmake', '--build', str(self.root / 'build'), '--parallel', '4'])
        self.assertEqual(built.returncode, 0, built.stdout)
        executable = self.root / 'build' / ('lua-smoke.js' if wasm else 'lua-smoke')
        ran = self.command(['node', str(executable)] if wasm else [str(executable)])
        self.assertEqual(ran.returncode, 0, ran.stdout)
        self.assertIn('bundled Lua 5.1 and BitOp execute successfully', ran.stdout)

    def test_native_sources_build_without_a_dependency_download(self):
        self.build_and_run()

    def test_wasm_sources_build_without_a_dependency_download(self):
        self.build_and_run(wasm=True)

    def test_corrupt_archive_is_rejected(self):
        for archive in sorted((self.root / 'cmake/vendor').glob('*.tar.gz')):
            with self.subTest(archive=archive.name):
                original = archive.read_bytes()
                archive.write_bytes(b'corrupt archive')
                result = self.configure()
                self.assertNotEqual(result.returncode, 0, result.stdout)
                self.assertIn('hash', result.stdout.lower())
                archive.write_bytes(original)
                shutil.rmtree(self.root / 'build', ignore_errors=True)

    def test_missing_archive_is_rejected(self):
        for archive in sorted((self.root / 'cmake/vendor').glob('*.tar.gz')):
            with self.subTest(archive=archive.name):
                original = archive.read_bytes()
                archive.unlink()
                result = self.configure()
                self.assertNotEqual(result.returncode, 0, result.stdout)
                self.assertIn(archive.name, result.stdout)
                archive.write_bytes(original)
                shutil.rmtree(self.root / 'build', ignore_errors=True)


if __name__ == '__main__':
    unittest.main()
