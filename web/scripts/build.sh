#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/../.." && pwd)
version=6.0.10
sdk="$root/.toolchains/emsdk-$version"
if [[ ! -d "$sdk/.git" ]]; then
  git clone --depth 1 --branch "$version" https://github.com/emscripten-core/emsdk.git "$sdk"
fi
if [[ $(git -C "$sdk" rev-parse HEAD) != a2b92777574c2feda07994cd4f1079a3dfc151f8 ]]; then
  echo 'Unexpected emsdk revision; refusing to build with a different toolchain.' >&2
  exit 1
fi
"$sdk/emsdk" install "$version"
"$sdk/emsdk" activate "$version"
source "$sdk/emsdk_env.sh"
if [[ -e "$root/.git" ]]; then
  git -C "$root" submodule update --init --recursive
elif [[ -f "$root/SOURCE-DISTRIBUTION.json" ]]; then
  echo 'Building an unpacked source snapshot with embedded submodules.'
else
  echo 'Missing Git checkout or source-distribution manifest.' >&2
  exit 1
fi
# A cache from an older SDK must not keep its compiler or object files.
configure_args=(-S "$root" -B "$root/build-web")
if [[ -f "$root/build-web/CMakeCache.txt" ]] && ! python3 - "$root/build-web/CMakeCache.txt" "$sdk" <<'PYTHON'
import pathlib, sys
cache = pathlib.Path(sys.argv[1]).read_text()
toolchain = next((line.split('=', 1)[1] for line in cache.splitlines()
                  if line.startswith('CMAKE_TOOLCHAIN_FILE:')), '')
raise SystemExit(0 if toolchain and pathlib.Path(toolchain).resolve().is_relative_to(pathlib.Path(sys.argv[2]).resolve()) else 1)
PYTHON
then
  configure_args+=(--fresh)
fi
emcmake cmake "${configure_args[@]}" -DENABLE_LTO=OFF -DSURGE_BUILD_TESTRUNNER=OFF -DSURGE_SKIP_WERROR=ON
cmake --build "$root/build-web" --target surge-xt-browser surge-web surge-juce-browser-check --parallel "${SURGE_BUILD_JOBS:-4}"
