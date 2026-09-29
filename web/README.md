# Surge XT: JUCE on WebAssembly

The browser migration preserves the C++ JUCE interface. There is no TypeScript
replacement interface. The engine, processor, widgets, skins, and advanced
editors remain the existing Surge sources.

**Development port, not a completed browser release.** Build success alone is
not a parity result. Native packaging remains available until reference captures
and the portable feature inventory pass. See the [source-derived inventory and
workflow requirements](parity/README.md) for the completion gate.

## Build

`bash web/scripts/build.sh` initializes submodules and installs Emscripten 6.0.10
at an exact emsdk revision under `.toolchains`. Downloads of portable Lua 5.1.5
and Lua BitOp are checked with SHA-256. Native CMake 3.24+, Python 3.9+, Git, and a C/C++
compiler must already be installed. Do not commit generated Wasm artifacts.
The SDK lives in a versioned directory; the build script refreshes a CMake cache
that still points to a different compiler.

The development server serves `build-web/web`, including configured copies of
`web/browser/*.js`. After changing those scripts, rerun CMake configuration or the
build before testing; a page reload alone does not update the generated copies.

The JUCE port creates a build-local overlay of the pinned `libs/JUCE` sources.
It does not dirty or replace the JUCE submodule. Overlay anchor checks deliberately
fail on incompatible upstream changes. Both browser and desktop editor targets
consume `src/surge-xt/sources.cmake`.

Development targets:

- `surge-xt-browser`: existing Surge processor and JUCE editor.
- `surge-juce-browser-check`: isolated JUCE paint/input diagnostic.
- `surge-web`: engine C ABI and Emscripten Wasm AudioWorklet bridge.
- `surge-wavetable-snapshot-check`: development-only Wasm check of pending wavetable ownership and export snapshots (runs under Node).
- `surge-engine-reference`: native engine rendering harness (`SURGE_ENGINE_ONLY=ON`).
- `surge-convolution-worker-check`: native FFT worker ownership and audio checks.
- `surge-effect-retirement-check`: bounded effect/metadata cleanup and shutdown ownership checks.
- `surge-airwindows-lifecycle-check`: direct selector defaults, stable processing and prepared suspension for every registered Airwindows processor.
- `surge-airwindows-selection-check`: private selector preparation and allocation-free adoption against the native selector path.
- `surge-effect-binding-check`: prepared parameter attachment across all 31 effect families, including all 77 Airwindows variants, at both sample rates.

The native development harness also accepts `--oscillator-type ID`. Its matching
offline C ABI, `surge_set_oscillator_type`, uses the original oscillator selection
and default-initialization path and requires exclusive engine ownership. It may
construct oscillators and load table data, so it is not an audio-callback API.
See the [oscillator fixtures](parity/README.md#oscillator-audio-fixtures) for
source-derived coverage, reproduction steps and outstanding statistical cases.
For controlled stochastic comparisons, the reference harness accepts
`--storage-seed UINT32` after `--oscillator-type ID`. The corresponding
`surge_seed_storage_rng` development API reseeds only the storage RNG before
notes; it does not change normal application seeding or seed every DSP generator.

`python3 web/scripts/serve.py` serves `build-web/web` on localhost:8080.
The diagnostic is `/surge-juce-browser-check.html`; the application is
`/surge-xt-browser.html`. This server is for local development only.
Production static hosting must serve HTTPS with these response headers on HTML,
JavaScript, Wasm, and other resources:

```
Cross-Origin-Opener-Policy: same-origin
Cross-Origin-Embedder-Policy: require-corp
Cross-Origin-Resource-Policy: same-origin
```

## Static development package

After building, run `python3 web/scripts/package-static.py`. It prints a directory
under `web/dist/<SHA-256>/`, with `index.html` as the entry point. The directory
name hashes its deterministic `distribution.json`, which records every packaged
file's size and SHA-256. Runtime changes produce another version directory; the
tool never overwrites an existing version and rejects a damaged existing copy.
Use `python3 web/scripts/package-static.py --verify web/dist/<SHA-256>` to check
the complete package before serving it.

The package includes the application, its support scripts, font data and all
manifest-referenced factory objects. Engine/paint diagnostics and unrelated build
files are excluded. Factory object sizes and hashes are checked before and after
copying. Staging failures are cleaned up before publication; completed versions
are published by a directory rename. Files use mode 0644 and directories 0755.
The packager should run after the build has finished, not concurrently with a
build replacing its inputs.

Serve `web/dist` with the development server's `--directory web/dist` option and
open `http://localhost:8080/<SHA-256>/` (or use another port if 8080 is occupied).
For hosted use, keep the complete version directory together, use HTTPS and the
isolation headers above, and retain older versions while their pages are open.
The generated `_headers` provides example static-host rules; when using a version
subdirectory, put those rules in the host's site-level header configuration.
Immutable caching applies to versioned assets, not a mutable landing page or
latest-version redirect. Wasm and JS need their normal MIME types.

This remains a **development distribution**: its manifest records the source
commit and dirty-worktree state, but does not claim to contain corresponding
source or complete dependency notices. The included root LICENSE alone does not
finish release packaging. Native-product retirement, the release workflow,
dependency notices, complete corresponding-source packaging and the parity gate
remain outstanding.

`python3 web/scripts/build-snapshot.py --jobs 4` creates a source archive, extracts
it into a fresh directory under `.toolchains/source-builds`, verifies that tree,
builds `surge-xt-browser` with the installed pinned SDK, and verifies the tree
again. It records the toolchain, build commands and hashes of every packaged
runtime/library input in `build-receipt.json`. The resulting version directory
includes that receipt and its source archive. Build directories remain available
for inspection; a failed build does not publish a package. No hosting is performed.

The packager also accepts `--source-archive <archive> --build-receipt <receipt>`
as a pair for an existing build. It checks the source digest and all recorded
build outputs before staging, then verifies the embedded archive and receipt
again before publication. `--verify` repeats those checks for a bound package.
The receipt is an unsigned record of the local workflow, not an independently
authenticated attestation. An included snapshot and matching receipt do not
complete the dependency-license audit, toolchain source audit or feature parity;
`correspondingSourceIncluded` therefore remains false.

The first full isolated run built and packaged snapshot `2ba117b99fedabd7d7872af3d341c14d6dc9e2b7e74e2320b7f3e9e5f8923edb`.
Its package verifies 5,775 files, including a receipt for 5,767 runtime/library
inputs and the source archive. All three distribution checks passed in **12.1
seconds**, including nine packaging tests, eleven source-export tests, and Chrome
startup, exact font hashes, factory patch switching and live AudioWorklet blocks
from that exact version directory. Startup does not fetch the bundled source
archive. Set `SURGE_DISTRIBUTION_PACKAGE` to an existing version directory and
`SURGE_DISTRIBUTION_ROOT` to its extracted source tree to repeat that smoke test.

Validation passed five Python integrity/publication tests and three Playwright
distribution checks. The real packaged application starts from a versioned URL,
mounts byte-identical font files, loads a factory patch and processes audio in the
AudioWorklet with silent host output. One exploratory run reported an aborted
`.data` request despite successful startup; later runs passed. The smoke test
retains any such cancellation as diagnostic evidence and requires exact mounted
font hashes; other failed requests are rejected. This is not physical-device or
production-host verification.

## Source snapshots

From the development Git checkout, `python3 web/scripts/source-archive.py` exports
the current worktree to
`web/dist/surge-source-<manifest SHA-256>.tar.gz`. Unlike `git archive HEAD`, it
includes modified tracked files, initialized submodule worktrees, and untracked
migration source files under `src`, `cmake`, and `web`. Ignored build/cache files
and untracked files inside submodules are excluded. Unexpected untracked file
types in the migration directories require review rather than silent inclusion.
Keep the worktree unchanged while the snapshot is being written.

The archive contains a `surge-source` directory with a per-file hash/mode manifest,
submodule revision information, and `VERSION_GIT_INFO` for builds without Git.
Run `python3 web/scripts/source-archive.py --verify <archive>` before unpacking.
The exporter also verifies the archive before publishing it, detecting files that
change while being copied. Timestamps, ownership and gzip metadata are normalized;
tests confirm byte-identical output for identical source inputs.

To check extracted sources before and after a build, run
`python3 web/scripts/source-archive.py --verify <archive> --tree <extracted/surge-source>`
from the development checkout. This verifies the extracted manifest's digest and
every recorded source's content, permissions and symlink target. It rejects
missing or added files outside `build-web`, which is reserved for generated
outputs and cannot itself be a symlink. Scripts run inside that source tree
should disable Python bytecode output (`PYTHONDONTWRITEBYTECODE=1`). The isolated
snapshot build above performs these checks and records the resulting file hashes.
Checking a source tree alone does not establish which sources built an existing
binary.

After unpacking, run `bash web/scripts/build.sh` inside `surge-source` with the
normal build prerequisites above. Embedded submodules replace the Git submodule
initialization step. Lua 5.1.5 and BitOp source archives are bundled under
`cmake/vendor`, with their original license notices. CMake verifies the pinned
hashes and extracts local archives; missing or corrupt archives fail the build
without a network fallback. The pinned Emscripten SDK installation still requires
network access and is not bundled. Local edits after
unpacking are permitted. A static package built without Git records its starting
snapshot digest and an unknown dirty state, rather than claiming it is unchanged.

The bundled scripting-source checks build and execute Lua/BitOp with both native
and Wasm compilers from fresh copies outside the repository, with download proxies
pointing to an unavailable endpoint. They cover Lua 5.1 environments, BitOp,
and rejection of either missing or corrupt archive. All four checks pass; seven
source-export checks pass, including exact archive bytes and rejection of an
unreviewed archive. Static packages now include both MIT notices under `licenses/`.

Rebuild validation extracted a real snapshot outside the Git checkout and built
both `surge-xt-browser` and `surge-web` from its embedded sources. This used the
already installed pinned SDK; a clean SDK installation was not part of the check.
All three distribution checks passed against that rebuilt copy, including five
source-export tests, five static-package tests, and Chrome startup, exact font
hashes, factory patch switching and AudioWorklet processing. The package also
recorded the extracted source manifest digest with an unknown dirty state.

These source snapshots support rebuild verification. They do not yet establish a
complete corresponding-source release bundle, dependency-notice audit, or a
verified association between a source archive and a separately built binary.

## Verification

`npm ci --prefix web && npm test --prefix web` uses installed stable Google
Chrome, not Playwright's bundled Chromium. Playwright is pinned to 1.63.0.
Start the local server first. Set `SURGE_TEST_URL` to test another local build.
If the host audio output is unavailable, use
`SURGE_TEST_SILENT_OUTPUT=1 npm test --prefix web`. This opts into Chrome's
silent audio output while retaining real AudioContext/AudioWorklet execution.
It does not verify physical output devices. The default still uses host output.

The application runs JUCE messages, audio startup, patch loading and wavetable
handoffs from a 16 ms control timer. Animation frames perform repainting only;
pausing them no longer prevents startup or patch changes. Chrome can still
throttle timers or freeze a page, so this is not a guarantee against browser
page suspension. The animation-frame test holds every frame, verifies startup,
menu/undo actions, live audio and patch changes, then resumes painting.

For local startup diagnostics, `SurgeRuntime.snapshot()` returns control-phase
timing peaks and the last 64 lifecycle/download events, with monotonic and wall
timestamps. `Module.ccall('surge_browser_control_state','string',[],[])` returns
the atomic patch queue, readiness, loader and audio handoff flags as JSON.
Diagnostics stay in memory and are never transmitted. Standard browser tests
attach these snapshots on failure; if the main thread cannot respond within a
second, the attachment records that instead of delaying teardown indefinitely.

Native reference capture:

```
cmake -S . -B build-reference -DSURGE_ENGINE_ONLY=ON -DSURGE_BUILD_TESTRUNNER=OFF -DENABLE_LTO=OFF -DSURGE_SKIP_WERROR=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build-reference --target surge-engine-reference surge-convolution-worker-check surge-effect-retirement-check surge-effect-binding-check --parallel 4
build-reference/src/surge-web/surge-engine-reference resources/data "resources/data/patches_factory/Templates/Init FM2.fxp" 48000 /tmp/fm2.f32
```

Validate every shipped FXP without changing synth state:

```
build-reference/src/surge-web/surge-engine-reference --validate-factory resources/data
```

Check concurrent formula display/audio contexts and their independent shared tables:

```
build-reference/src/surge-web/surge-engine-reference --check-formula-contexts resources/data
python3 web/scripts/check-formula-contexts.py --build build-reference
build-reference/src/surge-web/surge-engine-reference --check-lua-compilation resources/data
```

The second command requires a Unix Makefiles native build and a compiler with
ThreadSanitizer. It recompiles the formula helper and reference harness with
instrumentation into a temporary directory, reusing the other native libraries.
It does not modify the build outputs. This is a focused cache-ownership check,
not a sanitizer audit of the entire engine or Lua interpreter. The fixture runs
1,000 evaluations per interpreter concurrently, exercises valid and cached-invalid
functions, verifies retained error details, and resets both shared tables midway.

The compilation check separates compilation from execution, transfers exclusive
interpreter ownership between threads, and verifies deferred top-level side
effects, function ordering, and syntax/runtime error stack contracts. Both formula
contexts also check retained voices, shared-reset timing, cached syntax errors,
and reclamation across 1,000 unused script replacements.

The output from the rendering command is interleaved stereo float32, three seconds, with C4 released after
one second. The block adapter adds 32 samples of latency. Captures must be
compared after accounting for latency and stochastic engine behavior.
Add `--convolution` before the data-directory argument to apply the deterministic
4096-sample stereo IR at 32 kHz used by the native/Wasm convolution fixture.
Use `--effect-type TYPE` to change slot A1 through the engine parameter path
before rendering. IDs come from the original engine/configuration metadata;
`surge_set_effect_type` exposes the same exclusive-ownership operation to the
Wasm comparison harness. Invalid slots/types leave the current state unchanged.
An optional `--effect-parameter INDEX NORMALIZED` after `--effect-type TYPE`
sets one parameter through the same engine path before rendering. The matching
`surge_set_effect_parameter` API rejects inactive parameters, invalid indices and
non-finite/out-of-range values without changing the selected effect.


## Effect comparison survey

`node web/scripts/effect-survey.mjs /tmp/surge-effect-survey.json` renders every
original effect family at 44.1/48 kHz using the native harness and browser engine.
Run it with the local server and both builds available. Optional trailing effect
family names select a subset; the report distinguishes that from a full survey.
It records binary and
fixture digests, Chrome version, native/Wasm error, difference from dry output,
and repeated-render differences. This diagnostic is not a full parity gate.

`web/parity/effects.json` records fixture coverage and the measured gaps for all
31 families. The regression suite requires meaningful changes from dry audio and
native/Wasm relative RMS error below `1e-5` for fixtures in 30 top-level families: 24 default/convolution
fixtures plus explicit edits for EQ, Exciter, Graphic EQ and Audio Input,
external-input fixtures for Vocoder, and active Airwindows subeffects.
Parameter indices come from the original C++ enums. Edited fixtures must differ
from both dry and default-effect output. Stereo input uses integer-period saws
with exactly representable amplitudes and must contribute to the output.
All four Vocoder modulator modes have external-input comparisons. Tape needs
stochastic signal comparisons. A matching fixture does not verify every mode or
parameter in a family; the expanded Airwindows inventory has remaining gaps.
The registry's measured default-survey results remain separate from its edited
fixtures; a dry default does not invalidate an active-parameter fixture.

`web/parity/airwindows.json` expands the original registry into all 77 streaming
entries. The 154-case native/Wasm survey records audio comparisons and native
and Wasm repeatability at both rates. Regression fixtures cover 61 active effects
and two retired placeholders; those original placeholders intentionally output
silence. AD Clip uses its Boost parameter to exercise processing instead of its
near-dry initial settings. Nine previously weak fixtures now apply source-derived
parameter edits: eight compare deterministically, while Deck Wrecka needs a
statistical comparison. Fourteen entries remain open: twelve require statistical
comparisons, and Dust Bunny and
To Tape have repeatable numerical differences at 48 kHz. This does not establish
all parameter, UI or real-time behavior within those effects.

Run the expanded diagnostic with:

```sh
node web/scripts/effect-survey.mjs /tmp/airwindows.json --airwindows --active-parameters
```

Append exact registered names to select a subset. The report identifies subsets
and keeps them distinct from the full inventory. Omit `--active-parameters` to
repeat the original default-setting survey. Edited cases record their ordered
parameter list and use `matched-parameter-fixture` when they pass. Streaming IDs, display names,
groups and selector fixtures are derived from the original Airwindows sources;
the regression suite rejects inventory drift or inconsistent coverage summaries.

The offline native harness accepts repeated `--effect-parameter INDEX VALUE`
options in order. Both harnesses resolve a pending Airwindows selection before
editing its controls, so the first block does not overwrite those edits with
new sub-effect defaults. Regression tests also compare edited fixtures with the
same selected sub-effect at its defaults. This does not change live effect
selection or complete its real-time construction work.

After the active-parameter expansion, **221 parity and serialization checks
passed in 3.8 minutes**, including all eight added sub-effects at both rates.
The native and Wasm harness builds, source inventory and whitespace checks passed.
The native CLI rejected five invalid trailing parameter values after a valid
preceding edit. The full suite now contains 438 checks; the last full run below
predates these 16 additional regression cases.

The initial 110 Airwindows regression checks passed. After adding the summary
validation, the complete audio parity/serialization suite passed **197 checks**
in 3.6 minutes. A separate 12-case diagnostic confirmed the report's active,
retired-silence, weak-fixture, statistical and numerical-mismatch classifications.
The later full browser run passed all 422 checks (see verification evidence below). The
source inventory and whitespace checks pass; the 14 pending Airwindows entries
and broader migration requirements remain open.

The diagnostic also accepts `--effect-only`. It feeds exact 32-frame stereo
blocks directly through the existing effect object, without voices, routing,
modulation or output gain. For the first second, left samples are
`((17*n % 257)-128)/256` and right samples are `((29*n % 193)-96)/256`; the next
two seconds are silent. Every input sample is exactly representable as a float.
`web/parity/airwindows-isolated.json` records this separate evidence: Dust Bunny
and AD Clip match exactly at both rates, and To Tape differs by about `1e-13`.
This suggests input dependence in the full FM2 comparisons; those end-to-end
mismatches remain open. The normal parity suite rejects isolated reports as a
replacement for its full-synth inventory.

After adding eight isolated-effect checks, the combined audio parity and
serialization run passed **205 checks in 3.6 minutes**. The browser and native
reference builds passed, and native Dust Bunny and To Tape full-synth captures
remained byte-identical after the harness change. The subsequent full browser
suite passed all 422 checks in 11.9 minutes.

The development C ABI `surge_render_effect_block` requires exclusive engine
ownership and exactly 32 frames. It shares the original effect implementation
and patch parameter storage, checks arguments and finite input before processing,
and supports an off slot as exact bypass. It is a comparison harness, not a
replacement for the live AudioWorklet path. Its native CLI counterpart is
`surge-engine-reference --effect-only [--effect-type TYPE] [--effect-parameter INDEX NORMALIZED] DATA_DIR PATCH.fxp SAMPLE_RATE OUTPUT.f32`.

## User files

`default-patch.spec.js` verifies **Set Current Patch as Default** for factory and
saved user patches across reloads. Factory and user patches named Init Sine in
the Templates category retain different oscillator-pitch values, proving that
startup honors the saved Factory/User distinction. Switching back to the factory
version and restoring Init Saw also pass (10 seconds).

Patch metadata defaults are covered by `patch-metadata.spec.js`: the original
Author and Comment mini-edit dialogs persist Unicode values, discard canceled
edits, and allow defaults to be cleared. The original-author attribution option
is toggled on/off/on with reloads; the Save dialog and serialized FXP contain the
expected author, comment and retained license. Canceling Save leaves the source
metadata intact. Clearing defaults restores the source patch's author/comment.
The five combined metadata and file-export workflows pass (30.5 seconds).

**Download this folder** exports the selected directory and its nested contents
through Chrome's directory picker. It creates a fresh folder in the selected
destination, adding a numeric suffix if needed; portable `.surge-skin` names keep
that suffix at the end. Existing destination files remain untouched. A failed
copy reports that some destination files may have been written, retains all
browser sources, and permits retry into another fresh folder. Canceling the
picker produces no export.

Folder tests export the complete PNG tutorial skin, check every file's SHA-256,
and reinstall the exported bundle through the original skin installer. They also
verify existing destination preservation, partial-write/abort handling, source
retention, retry and cancellation. All six combined file/folder workflows pass
(18.6 seconds); file and directory picker handles are mocked in these tests.

The browser **User files** button opens the persistent user directory. Open
**Patches**, then the category chosen in the original Save dialog, and select
**Download …** to export the FXP through Chrome's save picker. The same view exports
other individual user assets. Parent-folder navigation stays within user storage;
Refresh files reloads the directory listing. Exports retain the stored source,
including after permission denial, cancellation or a failed destination write.
The selected bytes are copied before the asynchronous picker opens, so a subsequent
save cannot silently change an export already in progress.

`user-files.spec.js` verifies the original Save dialog with Save Tuning on and off,
author/comment metadata, persistence after reload, byte-identical file export, and
reimport of the exported FXP. Additional cases cover cancellation, permission
denial, write failure/abort, retry, parent navigation, focus restoration and a
concurrent newer save. All 13 combined user-file, storage and static-distribution
checks pass (26.1 seconds). The dialog was visually inspected in desktop Chrome;
system save handles are mocked in automated tests, so real picker interaction
remains a manual requirement. The versioned static package includes the new view.

JUCE asynchronous file dialogs use Chrome file and directory pickers. Imports
are copied into the browser filesystem before returning the selection to JUCE.
FXP imports undergo the engine's structural validation before being exposed to
JUCE: header lengths, XML, embedded wavetable bounds, and compressed extension
containers are checked without modifying the current synth or editor state.
Exports use the selected browser handle; a failed disk write leaves the generated
file in browser memory and shows an error. Directory exports request write access;
directory imports request read access.

`/user` uses IndexedDB through IDBFS. Startup restores this filesystem before
Surge constructs its storage. Writes are serialized and automatically saved;
failures keep the in-memory copy, display an error, and expose **Retry saving**.
A database that cannot be opened is not overwritten by an empty filesystem.
Closing the tab with unsaved changes triggers Chrome's unsaved-work prompt when
Chrome allows it. Browser storage is local to the origin and browser profile;
export files you need to keep independently.

## Portable skin folders

All 11 shipped factory skins appear in the original Skins menu, including its
Tutorials submenu. Startup installs their XML metadata from the compressed
factory index. Images, PNG zoom variants, and fonts are fetched only when a skin
is selected, with at most four downloads in flight per bundle. Every file is checksum
verified and written into a separate staging directory before the bundle is
published and the editor switches skins. Failed downloads leave the current
skin and saved preference intact; selecting the skin again retries the download.
Changing the selection while a download is pending prevents the old request from
changing the editor or preference. The saved factory selection is restored
asynchronously after reload, using verified cached assets when available; the
embedded Classic skin remains visible while loading.
Browser checks select every shipped skin through the original menu and compare
all 186 installed files against the manifest hashes. They also cover restoring
a selected font skin from cache with asset downloads blocked, retry after a
failed font download, and a delayed request superseded by a Classic selection.

The original Skins > Install a New Skin command uses Chrome's directory picker
for `.surge-skin` folders. Files and nested asset folders are copied into browser
storage before the existing installation confirmation and skin loader run.
Selecting the installed skin from the Skins menu persists the preference across
reload. Browser tests install the shipped image/color tutorial, verify exact SVG
bytes after reload, check the selected skin, and confirm its waveform color in
the rendered canvas. A folder-read failure reports an error without installing it.
The shipped PNG and font tutorials also retain every imported file byte across
reload. The font test removes the imported TTF files in an isolated browser
context, verifies that the patch-title rendering changes, then restores the fonts
and verifies the original title pixels return. This establishes font use, not
native/browser glyph parity at every zoom or display scale.
Folder installs parse a candidate skin with separate image/font state before
committing staged files. Invalid XML, a missing skin root, and missing required
sections leave the installed skin and selection intact. Folder replacements use
the same rollback transaction as archives; a partial-replacement test verifies
all original bytes and selection survive reload.
ZIP skin drops also use the original installation confirmation. Tests install a
deflated archive with nested SVG resources, select the resulting skin, and verify
its exact assets and selection after reload. Cancellation creates no skin folder;
invalid/unsupported archives now show an error instead of only logging to the
console. Browser archives extract into staging directories before touching installed files.
Each staged skin is parsed with the same isolated candidate loader used for
folder imports before any archive category is committed. Mixed-archive tests
reject malformed XML, a missing skin root, and missing required sections with
zero file commits; original patches, wavetables, and skins survive reload.
Commit renames keep original files as backups on the same browser-storage mount;
a failed commit restores replaced files and removes new files. If rollback itself
fails, the staging directory is retained and the error identifies its recovery
path. A test injects failure on the second skin-file replacement, verifies exact
restoration and staging cleanup, then reloads to verify the original skin and
files persisted. A recovery manifest records each backup's original destination
before commit. Mixed-archive tests cover patch, skin, and wavetable installation,
late failure restoring all three families and removing new directories, and a
second failure during rollback retaining the original patch backup and its path
mapping across reload. A Recover archive files button lists retained originals by
their destination path and downloads them with their original filenames. It is
available after reload and also when saving browser storage fails. The test drives
an actual failed rollback through reload, verifies an exact original-file download,
retries a download error, and checks that the retained backup is still present.
Restoring downloaded files into their installed locations remains manual.

The tests use mocked directory handles and browser DataTransfer events. Real
picker/OS dragging, complete font/PNG visual parity at all zoom/display scales,
and validation of every skin asset and archive content family still need parity
verification.

## Tuning imports

The original SCL and KBM menu imports validate the candidate scale/mapping pair
before replacing the current tuning. Parse or tuning-construction errors report
failure and retain the current scale and keyboard mapping. Shared engine file
loaders use the same validation order. Browser tests import a five-note scale and
432 Hz keyboard mapping, reject malformed SCL/KBM files through the original
menus, and compare the complete tuning report and editor text before and after.
Both regression cases failed before the correction and pass afterward.

The original Factory Tuning Library menu and the tuning editor's Tuning Library
button open a searchable browser dialog listing all 200 factory tuning resources.
Downloads reuse the integrity-checked factory cache and fetch only selected files.
Scales, mappings, and documentation retain their original bytes and filenames.
Downloaded SCL/KBM files can be loaded with the existing tuning import commands.
Failed downloads remain retryable in the dialog. Browser tests cover both entry
points, complete catalog listing, lazy downloads, byte-for-byte SCL/KBM/document
exports, and failure/retry without altering tuning.

Canvas file drops now use the same staged, persistent import path as file pickers,
then pass peer-relative coordinates and file paths to JUCE's existing drop-target
routing. Removed peers and superseded asynchronous reads do not receive a late
drop. Browser tests cover valid SCL/KBM drops, malformed dropped tunings retaining
the scale/mapping/pitch report, and read errors preserving current state. They use
browser DataTransfer events. Additional tests verify FXP drops and corrupt-patch
rejection, exact WT/WAV frame samples and corrupt-table rejection, and a delayed
older file read losing to a newer patch drop. The WAV fixture has four distinct
frames to detect frame loss or reordering. Physical OS dragging,
modulation presets and skin archives still need workflow coverage. Real-time tuning handoff and the remaining
tuning editor operations also need parity work.

## MIDI input

Select **Enable MIDI** to request Chrome MIDI access, then select a device or
**All MIDI inputs**. Channel messages retain their channel numbers and enter
Surge's original MIDI handler, including notes, pitch bend, channel/poly pressure,
CCs, and program changes. MPE and MIDI-learn configuration remain in the JUCE
interface. Enable audio before playing. Permission and device-open failures are
shown with retry instructions; they do not replace the current patch.

The browser main thread produces timestamped events into a fixed 1,024-entry
single-producer/single-consumer queue. The audio callback copies due events into
a preallocated JUCE MIDI buffer. Queue overflow requests all-sound-off rather than
silently losing a note-off. Device changes, disconnects, **Disable MIDI**, and
**All notes off** also request note cleanup. A selected disconnected device does
not silently switch to another input. MIDI port selection persists across reloads. Reloading does not request MIDI
permission or enable input; a missing saved device never falls back to all inputs.
Failed preference saves retain the active selection and offer retry. Invalid
saved identifiers are reported and preserved until an explicit device selection
replaces the setting.
System-exclusive and system-realtime messages are not forwarded because Surge's
existing processor does not consume them; tempo and playback use application
transport controls.

Queue tests exercise real concurrent producer/consumer threads, future/late
messages, ordering, capacity, and overflow recovery. Browser MIDI tests simulate
ports and permission failures. An offline integration test also sends scheduled
notes through the original JUCE processor and checks generated audio and panic
recovery. Real AudioWorklet checks also exercise queued notes and panic at
44.1/48 kHz. Physical MIDI/MPE hardware and MIDI-learn workflows remain unverified.

## Audio lifecycle

A startup timeout leaves the original worklet import pending. **Enable audio**
continues waiting on that import instead of allocating another context or reusing
its stack concurrently. A late successful import can finish the original request.
After a definitive initialization failure, retry waits for `AudioContext.close()`
to resolve before removing the old Emscripten handles and starting a new context.
Repeated clicks cannot overlap that cleanup. If closure fails, no replacement is
created; the error remains visible and cleanup can be retried.

Imports belonging to a context being closed are rejected before Emscripten can
construct a late bootstrap node. Bootstrap ports and state-change handlers are
released after successful closure. Browser suspension/interruption is reported
separately from readiness; **Enable audio** resumes the existing worklet. Pending
MIDI notes are cleared when processing stops, and new MIDI events are rejected
until audio resumes.

The C ABI audio status values are 0 (idle), 1 (starting), 2 (running), 3 (closing),
4 (suspended/interrupted), and -1 (failure or a still-pending startup timeout).
Lifecycle tests exercise the real Wasm entry points with simulated Web Audio
objects, plus real Chrome context closure. Separate real-worklet tests render notes,
change patches while processing, exercise download failure/retry, resume after
suspension, and restart after a processor-error event at 44.1/48 kHz. The tests
connect an analyser through a zero-gain node, so verification does not play sound
through the speakers. Error-event injection verifies the recovery path, not
recovery from every possible DSP fault.

Emscripten 6.0.10 supplies the pthread metadata needed by existing JUCE/engine
locks inside the worklet. `BrowserAudioThread.c` isolates a version-checked SDK
internal dependency: unlinking that metadata only after Chrome confirms context
closure, before the static stack can be reused. A toolchain upgrade must review
that shim. Random audio generators use a synchronous browser seed source, and
the CPU meter uses Emscripten's coarse worklet clock fallback. Neither performs
filesystem work or requests an unavailable monotonic clock in the callback.

A live patch change fades and halts the engine before publishing a pending load.
The main thread starts the background loader; the callback outputs silence while
the loader owns the engine and keeps queued performance events for resumption.
This handoff has live coverage, but a complete allocation and control/state
ownership audit is still required.

Restart preparation also waits for the loader before changing sample rate or
warming up effects. Waiting does not block the browser event loop, and the new
context is resumed before the wait so user activation is retained. A regression
test holds the actual loader worker, restarts from 44.1 to 48 kHz, then releases
the worker and checks the rendered A4 pitch. The patch-name API retains its last
committed snapshot while the loader owns the mutable patch state.

## Audio input

**Enable input** starts the audio graph, then requests microphone/audio-device
permission. The selector lists available input devices after permission is granted.
Capture requests stereo with echo cancellation, noise suppression, and automatic
gain control disabled, and connects only to the worklet's input bus. There is no
separate direct-monitor connection to the speakers. Use Surge's audio-input
oscillator templates to process the incoming signal.

A device change prepares and connects its replacement before releasing the old
stream. Permission or connection failure retains the previous input and patch.
**Stop input**, device disconnect, and page exit disconnect the source and stop
all captured tracks. Stopping while a permission prompt is pending also stops
any stream that arrives afterward. Input device choice persists across reloads,
without automatically starting capture. Failed preference saves offer retry.

Browser tests simulate media devices to verify permissions, switching, cancellation,
disconnection, and track cleanup. The factory stereo audio-input patch also
processes supplied stereo samples through the original JUCE processor in offline
checks at 44.1 and 48 kHz. Additional tests use Chrome's simulated capture
device through real getUserMedia and AudioWorklet processing at both rates,
verifying stereo signal routing, permission retry, replacement, and cleanup.
Physical audio devices remain unverified. Reference: https://www.w3.org/TR/mediacapture-streams/

## Application transport

Embedded patch tuning now passes the existing tuning parser and combined
scale/mapping construction during FXP preflight, before the patch loader can
change synthesis or tuning state. Malformed embedded SCL and KBM data are rejected
with an import error; the current serialized patch stays unchanged and rejected
imports leave no published user file. All 3,561 factory patches pass the updated
native validator. The 21 combined patch-validation, tuning-import and live
recall regressions pass (1.4 minutes), including valid embedded tuning at both
44.1 and 48 kHz. Browser and native reference builds pass.

Patch tuning recall is verified separately in `tuning-recall.spec.js`. All four
combinations of keeping/overriding scale and keyboard mapping are selected through
the original menu and persist after reload. A five-note scale at a 432 Hz reference
and an embedded seven-note scale at 444 Hz make each choice observable in both
the original tuning editor and rendered pitch. The real worklet passes at 44.1
and 48 kHz; subsequent patches without embedded tuning retain both active values.
The eight full preference-action workflows pass (52.7 seconds), alongside the
six tuning-import/failure cases in a preceding combined run (14 checks, 1.2 minutes).

`tempo-recall.spec.js` exercises the original **Patch Settings > Tempo on Patch
Load** submenu through both persisted preferences and real audio at 44.1/48 kHz.
Keep Current Tempo retains 147.5 BPM when importing a patch saved at 192.5 BPM;
Override recalls 192.5 BPM. Revision-22 patches retain the current tempo, while
revision-23 patches without `tempoOnSave` use the native 120 BPM default when
Override is selected. Both choices and their checked menu states survive reload.
The combined nine tempo-recall and transport checks pass (30 seconds).

**Play transport**, **Pause transport**, and **Rewind** control the browser
playhead. Tempo accepts 1–999 BPM, including fractional values; meter supports
1–32 beats per bar and power-of-two beat units through 32. Play requests audio
activation from the same user gesture. Pausing freezes musical position while
the synth can still respond to notes. Rewind works while paused or playing.

The browser playhead feeds the existing JUCE processor's tempo, PPQ position,
playing flag, and time signature. Position advances only by rendered samples,
so background animation throttling cannot change musical time. The original
JUCE tempo field submits a command to the audio thread. Patch tempo recall still
honors the original preference. Native plugin and standalone transport behavior
is unchanged.

The original JUCE tempo field has an explicit browser accessibility title. Native
commit callbacks may release JUCE focus while its DOM text field remains focused;
subsequent explicit text edits and key actions restore native focus. The editor
also defers a queued tempo refresh while that field has focus, preventing the
previous engine value from replacing an in-progress edit. Ten repeated workflow
checks at 44.1/48 kHz pass: consecutive edits, bidirectional tempo display updates,
fractional engine tempo with integer display in the original three-digit field,
zero rejection, and both 1/999 BPM limits. These checks use the same processor's
exclusive offline rendering path; live-device timing remains separate evidence.
All 56 transport, text-input, accessibility, clipboard and shortcut-editor
regressions pass (1.5 minutes) after the focus and refresh fixes.

The browser engine exposes an exclusive offline session through
`surge_browser_offline_begin(sampleRate)`, `surge_browser_offline_render(frames)`,
and `surge_browser_offline_end()`. Rendering returns stereo signal energy and
uses the same processor, MIDI queue, and transport code as the worklet callback.
Lengths must be multiples of 32 and no more than 192,000 frames per call.
`surge_browser_offline_render_input(frames, left, right)` accepts planar float32
input buffers; a missing right buffer duplicates the left input.
`surge_browser_offline_state()` reports the transport state during that session.
Offline sessions are rejected once an AudioContext exists, preventing concurrent
access with a worklet. This is a development verification interface, not an audio
export workflow. Tests verify the original JUCE processor at 44.1 and 48 kHz;
live worklet scheduling remains unverified.

## Factory asset delivery

The `surge-factory-library` build target publishes all 5,756 files under
`resources/data` into `build-web/web/library`. The manifest includes original
paths, searchable names/categories, byte lengths, and SHA-256 digests. Content
objects have hash-based URLs; the application target depends on this publisher.
The library totals 492,668,211 source bytes and is not embedded in startup data.

`web/browser/library.js` provides asynchronous search, fetch, integrity checking,
Cache Storage reuse, and atomic installation into `/factory`. Concurrent requests
for one file share a download. Corrupt cache entries are discarded and fetched
again; failed downloads do not overwrite existing filesystem data. Cache quota
failures report a warning while retaining the verified bytes for use.

The original JUCE patch menu now receives all 3,561 factory and third-party
patches. A compressed index contains their unchanged XML metadata (about 9.2 MB);
native patch search reads this metadata instead of downloading patch audio data.
Startup also downloads the chosen initial patch and initial wavetable. Selection prepares the full FXP
asynchronously before the engine consumes its queue. Failed downloads retain the
current patch, and stale completions cannot replace a newer selection.

The metadata index also supplies the original factory preset scanners with all
295 effect and 130 modulator XML definitions. These 406,213 source bytes add
50,582 bytes to the compressed index; preset menus do not trigger hundreds of
individual definition requests. Audio assets remain separate. Browser tests
verify every definition against its manifest hash, apply a factory Reverb 2
preset with its original values, and exercise LFO preset selection and undo/redo.
Representative Envelope, MSEG, Step Seq, and Formula presets are loaded, undone,
redone, saved through the original menu, persisted, and reloaded. Their envelope,
segment, sequence, and Lua data are compared with the source definitions using
the native serializer's existing legacy-MSEG marker conversion.

The filesystem catalog contains zero-byte directory entries for unloaded FXPs.
These are index entries, not usable patch files: the browser preparation gate
must complete before native patch loading. The standalone C ABI uses ordinary
local files when this catalog is absent.

The wavetable catalog includes 931 tables and scripts. Small script definitions
are included in the metadata index; binary tables download on selection. Tables
are decoded into temporary storage on the UI thread, then published at an audio
block boundary by swapping preallocated buffers. Old buffers are reclaimed on
the UI thread. Request tokens reject late downloads and replacements invalidated
by a patch change. Failed downloads and invalid tables retain the current table.
Factory skin bundles use the on-demand selection path described above.
The impulse catalog exposes all 449 factory and third-party FLAC responses to
the original convolution picker using unloaded file entries. Selecting a response
downloads and verifies that file, then decodes it on the UI thread before queuing
the existing FX reload. Cache reuse, missing/corrupt-download retry, superseded
selections, clearing the response, and replacing the effect during a download
have browser checks. Decode failures report an error and retain the current IR.
The browser file picker rejects undecodable inputs without changing the current
response, supports a valid retry, and ignores completion after its target control
has been destroyed. Offline rendering at 44.1/48 kHz produces a convolution tail
after note release, with a silent control interval after the IR is removed.

IR download requests currently belong to the displayed convolution control:
rebuilding or replacing that control cancels a pending request. Preserving an
in-flight selection across unrelated view rebuilds still needs refinement.
Automatic preparation of filename-only IR references in patches/presets also
remains unverified. Resampling, trimming, reversal, normalization and FFT setup
now form a separately owned `ConvolutionKernel`, prepared only from sample spans
and copied settings without accessing live synth state.
`ConvolutionKernelWorker` provides 32 bounded single-producer
mailboxes: 16 for parameter edits and 16 for effect reloads. Its worker owns FFT
construction, failed/stale results, retired kernels
and submitted sample references. The producer swaps only a matching generation;
a busy mailbox rejects submissions without replacing its outstanding request.
The current worker limit is 4,194,304 input frames per IR. Metadata inspection of
all 449 shipped FLAC responses found a maximum of 864,000 frames, so none exceeds
that limit; this does not verify every response's decoded audio or DSP output.
Native tests compare actual left/right FFT output with synchronous preparation,
exercise all slots repeatedly, retain the current kernel after stale or invalid
requests, and observe sample-buffer destruction on the worker, including shutdown
and cancellation when no consumer remains.
Live convolution parameter edits now use this service. The current kernel keeps
processing while a replacement is prepared. Each processing block checks the
request generation and settings before swapping; obsolete results are discarded.
Reset/removal cancels pending work without retaining an effect pointer on the
worker. Preparation failures retain the current kernel and queue a UI-thread
error report. Offline rendering and control-thread preparation remain synchronous.
Chrome checks at 44.1/48 kHz exercise size/start edits, continuing playback and
patch replacement after an edit; the worker tests cover cancellation and stale
results independently.
Live effect reloads with already-decoded IR data now prepare the replacement
kernel before changing the current effect. Completion must match the current
sample-buffer ownership, sample rates and kernel settings. A failed preparation
retains the current effect. The commit transfers the old kernel to the worker;
clear/non-convolution replacements also defer until they can retire the old
kernel there. IR import/clear, single-effect and chain preset load/paste writers and copy
snapshots share the reload mutex, which audio only tries without waiting. UI notifications
run after releasing the writer lock. The reload-needed flag is atomic.
Ordinary live reloads now transfer the previous effect and its userdata map to a
separate bounded retirement worker (one lane per FX slot). An occupied lane
defers the reload before consuming a prepared kernel, retaining the current
effect. Publication only swaps ownership and sets an atomic flag; the worker
destroys the effect, sample references and hash buckets. Convolution effects
detach their request lane before publication, so a delayed destructor cannot
cancel the replacement's work. Native tests cover a full queue, rejected-transfer
retention, reuse, destruction-thread ownership and shutdown draining. Live
drag/scene tests also observe the browser worker's retirement counter.
Ordinary live reloads now construct the replacement instance and establish its
parameter metadata on the control thread, using private parameter/value storage.
There is at most one staged construction per slot. Audio adopts it only when
type, values, flags, MIDI mappings, asset ownership and sample rate still match;
superseded constructions are reclaimed on the control thread. Convolution
filename decoding performed by the constructor also runs there, and rejected
construction retains the current effect. The control thread holds the loader
ownership lock during construction so loader/sample-rate setup cannot mutate
the global storage it reads. Live Chrome checks select all 31 families through
the original FX menu at both rates and observe adoption plus continued playback.
Replacements now also normalize their private preset values and initialize their
processing state on that control thread; adoption rebinds their pointers without
initializing them again. Convolution prepares its filters separately from the
worker-built IR. Attachment swaps in that kernel and records its settings without
repeating filter initialization or constructing an IR. Default/forced reload
paths and audio-thread effect-type automation still need work. The direct
control/offline setter publication fix is described below. Prepared replacements
now run their reload hook on the control thread too, so Airwindows constructs
the selected sub-effect before attachment. Audio does not repeat that hook or
renormalize the resulting sub-effect metadata. Direct Airwindows selector edits,
suspension and default/forced paths still need their own lifecycle audit.
Other FX writers and the complete concurrency protocol also need further audit.
Full real-time allocation and deadline audits remain unverified.
Prepared adoption now transfers parameter metadata with moves instead of clearing
and copying its heap-owning strings and optional SST metadata on the callback.
`Parameter` explicitly retains its copy operations and supplies move operations;
compile-time checks require non-throwing moves for adoption. The old metadata
remains in the consumed preparation object for control-thread cleanup. Off slots
also prepare their inactive labels and values on the control thread.

The native attachment harness uses heap-backed outgoing strings and engaged
optional metadata, counts scalar C++ `new`/`delete` operations on the transferring
thread, and verifies a positive allocation control before the no-allocation
check. All 214 preparations and the additional Off transfer pass; it also checks
that old metadata ownership moved into the preparation object and processes the
new effect after that object is destroyed. This probe does not intercept direct
`malloc` calls or certify the entire callback. Modulation restoration, other reload
paths and the broader concurrency/allocation audit remain open. Four Reverb 2
and Resonator native captures at 44.1/48 kHz stayed byte-identical after the move
support was added.
Both Wasm targets built, and the subsequent 248-check run passed in 6.2 minutes:
live adoption of all 31 families and clearing to Off at both rates, Airwindows
preset metadata, chain/reorder/scene workflows, failed-preset retention,
convolution workflows, native worker/attachment checks and existing deterministic
effect audio comparisons. The native allocation probe is limited to the metadata
transfer; this is not a claim that all effect changes are allocation-free.
Browser FX replacement also removes routes by destination ID instead of querying
which modulators are valid after replacement. This clears routes to newly inactive
parameters (including Off) and avoids temporary modulation-index vectors. The
native `surge-fx-modulation-check` exercises every slot, inactive destinations,
both source scenes, multiple indices, muted routes, notifications and preservation
of unrelated routes and vector capacity without observed C++ heap operations.
The browser `fx-modulation-cleanup.spec.js` checks serialized route identities
through replacement, undo/redo, clearing to Off and restoration at offline,
44.1 and 48 kHz. It also checks saved patches reach the search index without a
database-error dialog. Its two-scene fixture uses scene LFOs: imported macro routes intentionally normalize to scene A in Surge.
Rapid saves exposed SQLite commit contention with the UI reader and statements
left unfinalized on query errors. Statement ownership now releases the SQLite
handle on every exit path, including errors returned by explicit finalization.
Browser index-writer connections now use a one-second SQLite busy timeout; only
the background writer waits, avoiding a main-thread wait that could block its
proxied filesystem requests. A refresh requested by a save while indexing is
active remains pending for the next control tick after the worker becomes idle. Persistent failures still use
the existing error reporting. This does not establish crash recovery or multi-tab
storage safety.
The recursive routing lock and listener callbacks still require the broader
real-time ownership audit; this cleanup check does not certify modulation
restoration or the whole audio callback as allocation-free.
Both Wasm targets and the native cleanup harness build after these changes.
The four cleanup/search checks pass (50.2 seconds), followed by 18 related FX
chain, preset-failure, reorder/scene and factory-selection/search checks
(1.7 minutes). These are focused regressions, not a complete parity run.

Effects now expose an engine-owner-only parameter-storage rebinding operation
for attaching a prepared instance. It updates both the direct value pointers and
the SST adapter's separate storage pointers; convolution also cancels its old
request lane before attaching to a live slot. The native preparation check covers
all 31 families and all 77 Airwindows variants at 44.1/48 kHz, initializes using private preparation storage,
releases that storage after rebinding, and then renders finite audio (including
a convolution IR). This establishes attachment coverage, not full sonic parity.
The complete shared-state audit, including random generators, tuning and tempo,
remains required; loader/sample-rate exclusion alone does not prove that audit.
Native checks also reject constructions after changed values,
flags, MIDI mappings, asset ownership, sample rate or prior consumption.
After moving non-convolution initialization, all three browser targets built and
**21 focused checks passed in 1.3 minutes**: native worker/retirement/attachment,
all-family live switching at both rates, FX drag/scene paste/undo, and convolution
editing, failed-download retention and delayed-selection invalidation. The native
attachment check covers 62 family/rate combinations. These checks do not establish
allocation-free callbacks, exact live-switch audio parity or deadline guarantees.
The expanded native check now passes **214 preparations**. For every Airwindows
variant it additionally requires the selected sub-effect to exist before attachment
and retain its identity through 128 audio blocks after private storage is released.
Four Chrome checks load DeRez and Cabs through the original preset menu during
playback at both rates, then export presets and compare their streamed parameter
values with the factory definitions. They pass with the prepared reload hook.
The combined worker, attachment, live-family, preset and FX reorder/scene checks
passed **11 tests in 47.2 seconds** after rebuilding all three browser targets.
Convolution's split preparation additionally matches ordinary synchronous
initialization sample-for-sample for 128 stereo blocks at both rates in the native
attachment harness. Native full-engine convolution captures are byte-identical
before and after extracting filter initialization. These fixtures do not establish
all convolution parameters or real-time deadline behavior.
All three browser targets built, and **151 regression checks passed in 3.5
minutes** after this split: worker/attachment checks, file-backed preset failures,
live and offline IR workflows, stereo convolution parity, and the existing
deterministic Airwindows comparisons at both rates.
IR selectors now capture a parameter baseline before queuing the reload. Newer
values, flags and MIDI mappings merge independently into that same pending IR;
changing kernel settings invalidates obsolete prepared results. Weak sample
owners identify the selection without retaining large buffers. Explicit preset,
paste and undo operations finish that merge transaction; Copy only snapshots the
current effect and leaves a pending reload intact. Undo captures the preceding
effect before publication. Chrome checks verify edits before the commit counter
advances at 44.1/48 kHz, then verify the resulting controls and live IR undo/redo.

Single FX presets decode filename-backed responses into a private candidate before
publishing a reload. Missing/corrupt files leave the current effect, preset label
and undo history intact. Decoding and error reporting happen outside the FX
publication mutex. Successful selection publishes once, including with inactive
audio; duplicate publication previously discarded the decoded response on the
second reload. Invalid effect-type IDs are rejected before preset-table indexing.
Chain presets use the same decoder and stage all four slots before publication.
A missing/corrupt response in any slot leaves every destination and its undo
history untouched. Chain type IDs are validated when scanning. This covers
file decoding failures; subsequent asynchronous kernel preparation and live DSP
commit are still per-slot rather than an atomic chain operation.

Chain paste and chain presets publish explicit reload flags for all four slots,
including replacements whose effect types stay the same. The browser captures
undo and writes the complete candidate chain under the FX reload mutex; copying
and saving also snapshot under that mutex. Saved snapshots own their data through
the asynchronous name and overwrite dialogs. Chooser context menus bind to the
newly selected slot before capturing callback destinations, without waiting for
the deferred editor rebuild. Chrome tests cover distinct values in all four
slots, repeated paste, per-slot undo, save/overwrite, persistence and loading into
another scene, with clipboard checks during playback at 44.1/48 kHz. This does
not make multi-slot DSP replacement atomic: slots with asynchronous convolution
preparation can still commit on different blocks. A chain readiness/commit
protocol and deadline profiling remain required.

Direct browser effect-type parameter edits now build their default parameter
candidate privately and publish it under the same FX mutex used by `loadFx` and
control-thread preparation. The committed patch type remains unchanged until
adoption; initialization failure cannot leave a partly initialized `fxsync`
candidate. The native setter remains unchanged. The direct-edit regression
visits every configured effect type at both sample rates, checks the committed
type before adoption, and compares resulting parameter metadata with immediate
offline adoption from an independently initialized engine. This protects this
control/offline publication path; it does not authorize calling the setter from
the audio callback or close the broader shared-parameter ownership audit.
Both Wasm targets built, and the subsequent 218-check run passed in 4.1 minutes:
the two direct-edit regressions, three live-worklet lifecycle checks and the
existing native/Wasm effect comparisons and serialization check. The direct-edit
test uses sequential, independently initialized engines; concurrent instances in
one diagnostic Wasm module timed out with its fixed worker pool. This result
does not close multi-instance support or the Airwindows callback construction
paths: `AirWindowsEffect::process()` calls `setupSubFX()` on selector changes or
missing metadata, and `suspend()` can call it when parameter metadata is absent.

The native lifecycle harness visits all 77 selectors at 44.1/48 kHz with poisoned
prior parameter values, checking that direct selection installs processor defaults
and removes metadata for unused parameters. A positive allocation control checks
the scalar C++ allocation probe, which observes eight post-selection processing
blocks and one suspension per processor and rate.
Before the suspension correction, the two retired DeEss/Tube placeholders each
reconstructed their stateless silence processor during suspension at both rates.
An already prepared `AirWindowsNoOp` with matching selector and valid metadata now
retains its instance. The harness checks unchanged instance identity, no observed
C++ heap operations and exact silence after suspension with nonzero input.
Uninitialized or mismatched placeholders retain their original setup path.
The native reference selector path still allocates in all 154 cases. The browser
request path below uses prepared adoption; this lifecycle check alone does not
certify direct `malloc`, the whole callback, concurrent edits or allocation-free
suspension of unprepared state.
Both Wasm targets and the native reference/lifecycle harnesses build with this
change. Nine focused checks pass: the 154-case native lifecycle check, four live
streamed-preset checks and four native/Wasm retired-placeholder silence comparisons
at both sample rates (21.4 seconds total).

`AirWindowsEffect::prepareSelection` now builds a processor and its default
parameter metadata privately. `adoptSelection` exchanges the processor and
heap-owning parameter metadata while retaining the running adapter, its smoothing
state and its value pointers. The consumed preparation owns the outgoing data
for control-thread reclamation; it rejects reuse and adoption by another adapter.
Active formatters refer to the retained adapter rather than the temporary builder.
The native selection harness compares all 77 processors at both rates with the
original direct path, including the first processing block's smoothing state,
parameter names/types/ranges/defaults, non-finite output checks, invalid IDs,
owner checks, reclamation before processing and C++ heap-operation probes.

The handoff also has a trivially copyable, fixed-size `SelectionRequest`. The
serialized engine owner captures values, MIDI assignments, takeover state and
edit flags without copying strings or optional metadata. The control-side overload
reconstructs a candidate from this snapshot and an immutable slot metadata basis;
it does not read the live parameter array. It validates the owner, selector,
sample rate and slot parameter IDs. Adoption rejects changed sample rates or an
intervening processor selection, and preserves the live oscillator-alias string
ownership because direct selection does not change that field.
The native harness deliberately gives preparation stale basis values/flags and
changes live values after snapshot capture, then compares its output with native
selection. It also checks wrong-slot rejection and obsolete rate/selection
rejection. Request capture joins adoption and first-block processing inside the
C++ heap-operation checks for all 154 cases.

Browser UI selector edits and active MIDI controller interpolation now publish a
requested selector instead of changing committed parameter metadata. A bounded
single-request mailbox transfers the fixed-size snapshot from the serialized
engine owner to the control loop. The control loop prepares it against `fxsync`
metadata under the existing FX lifetime lock; audio never waits for that work.
Audio consumes only a still-current request, preserving raw modulation routes and
adapter smoothing state. A later MIDI-assignment/value/flag edit rejects the old
snapshot and causes recapture. Preset/family replacement clears the old instance's
selector request. Consumed or rejected processors and metadata are reclaimed by
the control loop (or the existing effect-retirement owner at instance teardown).
Preparation failure retains the committed processor and reports an error.

When audio is suspended, the inactive engine control pump also captures and
adopts selector requests. The committed processor and parameter controls update
without waiting for playback to resume, and retired state is still reclaimed by
the control loop. This also lets already-published requests finish after audio
stops. The browser regression reproduced stale parameter controls before this
change; at both sample rates it now verifies suspended selection, saved AD Clip
defaults and modulation routes, suspended undo/redo, unchanged audio-block count
while stopped, and resumption without additional callback constructions.
After this correction, both Wasm targets and the two native Airwindows harnesses
build; 16 focused native/browser checks pass (35.3 seconds), covering the expanded
selector workflows, audio startup/teardown failure recovery and real-worklet
playback/resumption at both rates. The run uses silent output and does not certify
physical audio or MIDI devices.

The mailbox stress check runs 2,000 requests across actual native threads, injects
preparation failures, verifies one-request capacity, checks destruction ownership,
and observes no C++ heap operations on the audio-side test thread. The actual
Airwindows harness also tests stale MIDI assignments and recapture/adoption for
all 154 processor/rate cases. Browser tests use the original selector, undo/redo
and MIDI Learn with an assignable CC (74 is reserved by Surge), preserve a muted
scene-B modulation route in serialized patches and check prepared-adoption counts.
A separate callback-construction counter detects any fallback construction during
those edits. This uses the real worklet with injected MIDI messages, not a physical
MIDI-device certification.

Undo snapshots now retain a pending Airwindows selector when the caller captures
the current value. Previously, immediate undo before preparation committed put
the old selector on the redo stack, losing the requested edit. The browser test
reproduced that failure by dispatching selection and Undo in one main-thread task
(preparation cannot run between them), then checking Redo. It exercises this
sequence during real-worklet playback and suspension at both sample rates.
Explicit snapshots of other values keep their caller-supplied value.
The original-JUCE Wasm target builds with this correction, and ten focused checks
pass (1.1 minutes): both expanded selector workflows, direct effect-type edits,
native routing cleanup, offline/live routing undo, and live FX drag/scene paste.

The browser patch-save dialog waits asynchronously for pending effect selections
and reloads before serializing. A regression reproduced the old selector being
written when selection and Save ran in one main-thread task. The dialog now keeps
its fields and original overwrite options while waiting, offers Cancel, and
checks patch-load generations, reported preparation failures and a 15-second
timeout before proceeding. Closing the overlay cancels its pending save; a
temporary detachment during editor refresh does not. The tests cover immediate
Airwindows saves during playback and suspension, exact saved defaults and routes,
and cancellation without a late file write at both sample rates.

The standalone offline engine API has no periodic control pump. Its selector
setters now finish the bounded capture/prepare/adopt/reclaim sequence under their
existing exclusive ownership; the native reference API also resolves selection
before returning. API tests exercise both generic and effect-specific setters,
follow-up parameter edits, and immediate serialization without rendering first.
Both Wasm targets and the native reference/harness targets build. Fifteen focused
checks pass (1.0 minute), covering the expanded save/undo/MIDI workflows, offline
API serialization, native Airwindows lifecycle checks, real-worklet recovery,
effect-type publication and modulation cleanup. The original failure case wrote
selector 15 (DeRez) instead of selector 0 (AD Clip); it now saves selector 0 with
the AD Clip defaults and preserved modulation routes.
All 126 existing native/Wasm Airwindows audio comparisons also pass (2.3 minutes)
after the offline selector timing change. This covers the surveyed fixtures, not
the separately documented remaining stochastic processors or all parameter values.
Repeated save testing also exposed a transient favorites read racing the index
writer. The resulting database alert supplied another OK button and interrupted
the save workflow. Browser favorites refresh now retains known paths and local
favorite changes while indexing, then retries from the control loop after the
writer drains. A busy/locked favorites query requests another refresh instead of
reporting a transient error or clearing favorites. The control loop excludes the
patch loader while refreshing. Save tests target the dialog's own buttons, check
native accessibility action results and reject database-error console reports.
Writer readiness also checks the queue under its lock: the worker's waiting flag
alone remains true between enqueue and wakeup and cannot prove that reading is
safe. The expanded workflow keeps a favorite through successive saves and reload.
The browser patch selector is now an accessibility focus container. Its search
and favorite child buttons are exposed, and their accessibility actions invoke
the original handlers instead of a no-op. The favorite button also exposes its
original context menu. This preserves the visual layout while making these
controls usable through the browser accessibility bridge.
After those corrections, all six repeated live workflows pass (2.2 minutes),
covering both sample rates three times, with no database-error reports. Each run
verifies immediate/suspended saves, cancellation, undo/redo, MIDI selection,
callback-construction counts and favorite persistence across reload.
Twenty related accessibility, offline API and patch-browser checks pass afterward
(38.3 seconds), including factory menu selection, cached metadata search, download
failure recovery, Unicode editing and stale accessibility identifier rejection.

Preparation-failure, patch-change and timeout cancellation paths for queued saves
still need end-to-end fault-injection coverage. Coherent snapshots during other
concurrent edits, suspension during an in-flight request, wider editor ownership
and default/forced reload paths still
need full workflow/concurrency verification. The original fallback setup path
remains for unprepared states and offline/native operation; the new selector path
does not establish that every effect callback is allocation-free.
Both Wasm targets and the native reference/check targets build. Seventeen focused
checks pass (59.5 seconds): both native Airwindows harnesses, live selector/MIDI/
undo workflows at both rates, streamed Airwindows presets, effect-worker and
attachment checks, every effect-family construction, direct type publication and
FX drag/scene workflows. The selector tests observe no increase in the callback
construction counter. A startup-only test race was corrected to wait for the
Wasm patch-name export before calling it.

Scene copy/paste and effect reordering now also hold the FX reload mutex while
capturing live effects and publishing replacement candidates. Replacements end
the affected slots' pending-IR parameter merges; a drag-copy leaves its source
transaction intact. Chrome tests at 44.1/48 kHz exercise mouse drag swap, copy and
move, scene paste, patch-based undo/redo and fresh-note playback afterward. This
protects FX candidate access only: the original scene-copy code still writes
other live scene state and uses the modulation and wavetable locks. It does not
establish an atomic scene transaction or a fully nonblocking audio callback.

Script generation uses the original Lua editor and generation worker. In the
browser application, the worker builds a private table and defers the result until
the control thread retires a block-boundary swap. The worker remains available for
previews and blocking export requests while publication is pending. The audio callback only attempts
the mutex and swaps buffer ownership; it does not generate, allocate, or destroy
tables. Old buffers retire on the main thread or generation worker. Publish tokens
reject replacements invalidated by a new patch/table selection; teardown resolves
deferred results while the generation service's accounting state is still alive.

Original-editor tests compare generated samples, retain the current table on a Lua
error, generate during live playback at 44.1/48 kHz, and export a WT file through
the original menu and browser picker (header, all samples, and Lua metadata checked).
WAV, Serum, and VCV Rack exports verify chunk boundaries, format fields, frame
resolution, all sample values, and embedded Lua metadata. Frame-directory tests
verify each file and its samples, repeated exports preserve existing destination
folders, and interrupted writes report errors while retaining generated frames
across reload. Exports snapshot script inputs, wavetable data, and metadata before
opening the picker. Tests hold the picker open across patch changes and subsequent
generation, then verify the original exported samples/script and unchanged new
live state. These tests use mocked browser file handles; actual system-picker
and disk behavior remain manual checks. Supersession, shutdown, and broader
scripted-editor workflows still need full parity coverage.

WAV exports use byte-based rate/alignment fields, an IEEE-float format extension
and sample-count chunk, and word-aligned metadata chunks. Chunk sizes exclude
padding while the enclosing RIFF size includes it, following the
[RIFF container rules](https://learn.microsoft.com/en-us/windows/win32/xaudio2/resource-interchange-file-format--riff-).

Script-file metadata imports stage snapshot data in a private oscillator before
committing it. Invalid XML retains existing snapshot objects, their version, and
script settings; corrupt compressed snapshot payloads now reject the import.
The native regression reproduced snapshot loss before the fix and passes after
it. Browser script-drop tests verify generated samples, malformed XML and corrupt
compressed payload rejection, and preservation of the original editor script.
A native fixture built with the desktop snapshot serializer imports with exact
sample values; Chrome imports that binary file and regenerates from its retained
snapshot after rejecting a replacement. The original desktop export UI and
broader snapshot-schema validation still need coverage.

## Clipboard and text editing

The browser overlay adapts JUCE TextEditor and CodeEditorComponent paste/cut
commands to Chrome's asynchronous text clipboard API. Keyboard shortcuts and
original JUCE context menus share the same path. Permission/API failures display
an error and retain editor text. Cut removes the selection only after a successful
clipboard write. Delayed results are discarded if the target was destroyed,
changed, made read-only, or lost keyboard focus. Paste/cut create separate undo
transactions. Transfers use Wasm heap memory, including scripts larger than the
call stack; no stale clipboard cache is used.

The tests mock clipboard permissions and data while exercising the compiled JUCE
editors, context menus, undo/redo, and Surge's patch-search field. Real OS clipboard
permissions and broader text/accessibility workflows still need verification;
composition support and its remaining gaps are described below. The synchronous JUCE clipboard getter has no browser equivalent;
all current JUCE consumers are replaced by checked overlay edits. New consumers
must use the asynchronous API rather than that empty synchronous getter. The
overlay generator rejects new non-native synchronous clipboard consumers.

## Browser composition input

The browser peer binds Chrome's [EditContext API](https://developer.chrome.com/blog/introducing-editcontext-api)
to the focused JUCE TextInputTarget. JUCE owns the text, selection, and undo history.
The bridge converts Chrome's UTF-16 offsets to JUCE codepoint indices, supplies
character/caret bounds for IME positioning, and paints composition underlines.
Composition stays within one undo transaction across normal editor timer pauses.
Read-only or unfocused targets detach; callbacks from replaced contexts are ignored.
Ordinary JUCE shortcuts and clipboard commands remain in use.

Chrome input-protocol tests exercise actual text insertion and composition events,
including emoji selection, cancellation, target switching, and candidate bounds.
These are not physical OS IME checks. The bundled fonts currently lack CJK/emoji
fallback glyphs: those characters survive editing, but display as missing-glyph
boxes. Font coverage and real input-method/candidate-window verification remain
required before international text workflows can be considered complete.

## Portable HTML reports

The shared JUCE report command opens a modal browser viewer with HTML download
instead of launching a temporary file through the OS. Report content runs in a
sandboxed iframe without script privileges. Closing restores editor focus; a
failed download retains the report and displays an error. The original all-parameter
patch export menu is covered end to end. Tuning, MIDI mapping, and skin reports
share this viewer but still need their own workflow verification.

## Accessibility bridge

The browser projects JUCE AccessibilityHandler metadata into Chrome's DOM and
accessibility tree. Labels, descriptions, roles, supported checked/selected/expanded
states, numeric ranges, and displayed values come from the original controls.
Snapshots run on the main thread every 250 ms; surviving nodes retain their DOM
identity. Button activation, toggles, context-menu actions, and ranged-value edits
call the original JUCE handlers. Sliders support arrow keys and Home/End following
[the WAI-ARIA slider pattern](https://www.w3.org/WAI/ARIA/apg/patterns/slider/).
A visible focus outline identifies the associated canvas control without intercepting
pointer interaction with the canvas.

Focused custom editor groups now forward keyboard events to their original JUCE
handlers, preserving modifiers. Events from child controls are not forwarded a
second time, and unhandled keys retain browser defaults. This fixes dropped
MSEG canvas arrow keys in the accessibility bridge. Browser tests load the Major
MSEG preset, navigate nodes with Tab/Shift+Tab, edit the first node with ArrowUp
and Shift+ArrowUp, and verify the serialized values (-0.95 and -0.99), undo/redo,
and exact MSEG state after saving a patch and reloading the page. The restored
editor rendering was also inspected. The related accessibility, text-input,
factory-preset and MSEG workflow run passed **36 checks in 1.0 minute**; both
JUCE browser targets built successfully. This establishes these workflows, not
complete MSEG mouse, segment-type, loop, selection or screen-reader parity.

The subsequent MSEG structural workflow run passed **23 checks in 1.4 minutes**:
the two node-edit/patch-reload cases above, node insertion/deletion with exact
undo/redo snapshots, and all 20 segment types through the original context menu.
Menu labels and streaming IDs are read from C++ source. Every type case checks
the selected segment's serialized type, unchanged types on the other segments,
and exact undo/redo state. The deletion fixture also checks the native merge
rule that resets control-point duration; it does not assume deletion reverses a
split. The dynamic segment-type menu now has a feature review listing all 20
runtime entries. The inventory contains 734 entries, 14 reviewed and 720 pending.
Mouse drawing, group selections, looping and audio behavior across MSEG types
remain separate requirements.

Exact-value entry and keyboard loop-boundary workflows exposed two further
issues. A keyboard-opened MSEG type-in did not capture its undo baseline, so undo
could restore an empty MSEG. Accepted edits now snapshot the current model before
mutation; rejected zero denominators and cancellation leave it untouched. Also,
DOM click/key events from accessible child controls no longer trigger their
parent's handlers, fixing the extra context menu opened by MSEG mode radios.
The combined MSEG, accessibility, text-input and factory-preset run passed
**59 checks in 2.5 minutes**, including repeated fractional value edits and exact
loop marker serialization with undo/redo. Both JUCE browser targets built.
Keyboard loop-start/end commands received inventory reviews in that run.

Six additional real-pointer cases verify mouse-context loop boundaries in both
Loop and Gated Loop envelope modes. At a segment midpoint, the menu selects that
segment; within its leftmost tenth, Set Loop End selects the preceding segment.
Each case compares the complete serialized model with only the expected marker
changed and requires exact undo/redo. All **33 MSEG and inventory checks passed
in 2.1 minutes**. Mouse-menu commands now have inventory reviews: 18 of 734 entries
are reviewed, with 716 pending. Marker dragging, degenerate geometry, loop audio
behavior and the broader completion requirements remain open.

Six mode-control workflows cover the radio, context-menu and keyboard routes
(`Alt+t` for edit mode and `Alt+l` for loop mode). An unequal-duration envelope
normalizes to one cycle in LFO mode and restores its timing when switched back;
each direction preserves exact undo/redo snapshots. All three loop-mode choices
change only the serialized mode and restore exact prior state. Two consecutive
passes completed **12 checks in 1.2 minutes**. The tests separate independent
gestures by 250 ms because the native undo manager coalesces edits to the same
MSEG within 200 ms, including edits immediately following redo. The shared
settings-menu inventory entry also contains movement and snap controls, so this
coverage does not mark that broader entry complete.

Thirteen further checks passed in **1.1 minutes** for segment flags. Use Deform,
Invert Deform, Filter EG and Amp EG toggle in both directions through their menus,
keyboard shortcuts and visible checkboxes. Trigger All and Nothing set or clear
both retrigger flags. Each test targets the middle segment, compares the complete
serialized model so other segments must stay unchanged, and verifies exact
undo/redo. Six corresponding menu commands now have inventory reviews, bringing
the count to 24 reviewed and 710 pending. Group-selection behavior and the audio
effects of deform/retrigger flags still require verification.

Group flag editing now has **16 passing checks in 1.1 minutes**. Keyboard range
selection (`Alt+s`, arrows, Enter) and select-all (`Ctrl+a`) are checked through
their accessibility announcements, then Use/Invert/Filter/Amp changes are applied
through both keyboard shortcuts and the visible checkboxes. Exact serialized
comparisons require every selected segment to change and unselected segments to
remain untouched. One undo restores the entire group and redo restores the edit.
Those fixtures start with uniform flags; mouse lasso, group geometry edits and
audio behavior remain separate verification work.

Twenty-four mixed-state group cases then exposed a stale-control bug. Keyboard
selection updated the canvas but did not refresh its flag checkboxes, so a group
toggle could use the previously focused node instead of the longest selected
segment. The selection callback now refreshes the controls, matching the existing
mouse-lasso path. Fixtures cover range/all selection with equal durations and a
longer middle segment, through keyboard shortcuts and checkboxes. Exact undo
restores the original mixed flags. Both browser targets built; the full MSEG
workflow run passed **90 checks in 6.4 minutes**, including all eight unequal-
duration cases that failed before the fix. Existing MSEG inventory reviews were
refreshed against the rebuilt and tested source; the count remains 24 reviewed
and 710 pending.

The Create menu adds **32 passing checks in 2.0 minutes**: Minimal, Default Voice,
Default Scene, all five step-sequencer counts, all five sine-line counts, and all
three sawtooth-pluck counts, each invoked from LFO and Envelope modes. Tests check
the generated segment counts, types, durations, starting values, flags and loop
markers, followed by exact whole-model undo/redo. The six source commands have
reviews with their runtime expansions; 30 entries are reviewed and 704 pending.
Generated-shape audio comparisons remain separate work.

The inventory now recognizes generated names with literal suffixes as dynamic
menus. A verified dynamic menu must list its runtime expansion; merely containing
a quoted string no longer classifies its label as static. Four inventory checks
pass, including rejection of a verified menu whose expansion is missing. The
existing skin-selection review also explicitly lists its factory entries and
the imported-user-skin family covered by its tests.

Fourteen transformation checks cover Double/Half Duration, vertical/horizontal
flip, both duration-quantization commands and even distribution in both editing
modes. Envelope-only commands are absent from JUCE's LFO accessibility menu.
Even distribution resets an LFO to one cycle and preserves an envelope's total
duration. The fixtures verify values, durations, segment flags and exact undo/redo;
curvature-specific mirroring and duration-limit cases remain unverified. Seven
additional menu reviews bring the inventory to 37 reviewed and 697 pending.

Testing those menus exposed a browser Escape-routing bug: opening an accessible
submenu could retire the focused DOM node, leaving subsequent keys on the document
body. The bridge now routes Escape through a live, unblocked JUCE popup root and
uses JUCE's dismissal API. Non-menu and retired targets are rejected. Both browser
targets rebuilt; **15 focused transformation/popup checks passed in 56.5 seconds**,
followed by **40 accessibility, text-input, editor and preset checks in 58 seconds**.

Popup navigation now also forwards arrow keys, Enter and Space to the active
JUCE menu. A reproduced ArrowDown failure came from the same lost DOM focus;
Left/Right traversal additionally required invoking the menu item's native focus
action before opening its submenu. The bridge now preserves that focus step for
menu focus and activation. Six focused checks pass: four real JUCE submenu
navigation/activation workflows with undo/redo, popup target guards, and an
isolated listener-lifecycle test using persisted page events. Listener restoration
is verified in that controlled test, not through a full browser back/forward-cache
round trip. Both browser targets built, and **56 broader input, editor, preset and
factory-skin checks passed in 1.5 minutes**.

Four follow-up checks passed in **22 seconds**. After Escape or Enter dismisses
a popup, ArrowUp immediately edits the MSEG without a refocus action. In both
LFO and Envelope modes, Link Edge Nodes synchronizes endpoints, linked final-node
type-in updates both ends, and unlinking restores independent final-node arrow
editing. Exact snapshots cover link, linked edit and unlink undo/redo. The free
final node retains the native announcement directing users to arrow keys instead
of type-in. This menu command now has a review: 38 entries reviewed, 696 pending.

Control-point type-in adds **58 passing checks in 3.8 minutes**, covering all 19
non-Hold segment types through context menus and `Alt+Enter`. Bezier, Brownian
Bridge and all eight ratchet types exercise both axes, including the keyboard
axis-choice popup. Fractional values change only the selected property; undo and
redo must restore exact serialized models. A separate Hold check passes, requiring
the native no-control-point announcement, no type-in or menu entry, and unchanged
state. Four control-point menu entries now have reviews and runtime expansions
where needed: 42 entries reviewed, 692 pending. Mouse gestures, boundary values
and audio behavior are not established by these checks.

Full-engine MSEG audio fixtures now pass **38 comparisons in 38.9 seconds**:
19 deterministic segment types at both 44.1 and 48 kHz. The fixtures route the
first voice MSEG to oscillator pitch in Init FM2, with three segments, 2 Hz rate,
0.2 deform and fixed control points. Each comparison requires identical repeated
native captures, relative native/Wasm RMS error below `1e-5`, and a measurable
signal difference both when removing the route and when changing segment type.
The browser also serializes the loaded patch to verify the actual MSEG type and
route before rendering. This guards against testing an inactive MSEG: streamed
parameter names are zero-indexed (`a_lfo0`), while the routing enum is `ms_lfo1`.
An initial fixture used the wrong parameter index; its passing comparisons are
not parity evidence and are superseded by these corrected checks.

`web/parity/mseg.json` records all 20 segment types. Brownian Bridge remains
`pending-statistical` for the immediate Brownian note-on fixture: repeated native
captures differ at both rates. A separate controlled-seed fixture is described
below. The passing fixtures do not establish
all mode, parameter, scene-LFO, envelope-release or retrigger behavior. Seven
existing dry, convolution, external-input and serialization regression checks
passed in **6.3 seconds** after the shared render-helper change. No production
DSP arithmetic was changed for these MSEG fixtures.

Brownian MSEG also has six controlled-seed full-engine comparisons: seeds 1,
17 and 123456789 at both rates. Its evaluator has an independent RNG, so the
storage RNG seed does not control it. The development-only
`surge_seed_voice_mseg` API (native harness `--mseg-seed UINT32`) seeds the
existing voice evaluator after note-on and before rendering; it rejects an
absent voice or a non-MSEG destination. It requires exclusive engine ownership
and changes future RNG draws, not already evaluated modulation state.

Voice construction evaluates one control block before returning. Seeding at
that point was too late for a fixture beginning directly with Brownian motion:
six native repeatability checks failed. The controlled fixture therefore places
a 0.125-duration Hold before three Brownian segments. Seeding then occurs before
the random walk starts. The browser round trip verifies all four segment types
and the pitch route. Tests require exact within-platform repeats, native/Wasm
relative RMS below `1e-5`, and audible differences both for another seed and for
removing modulation. Normal application seeding is unchanged. Immediate Brownian
note-on, natural-entropy statistics and other mode combinations remain open.
Both development harnesses built. The combined run passed 60 checks in 1.4
minutes: 38 original MSEG cases, six controlled Brownian cases, 12 seeded
oscillator cases and four dry/convolution comparisons. Both seed options rejected
six malformed inputs before rendering. The original immediate-onset Brownian
entry remains pending; its additional controlled fixture is recorded separately.

Bridge identifiers use safe component references and are retired when controls leave
the accessible tree. Actions check current visibility, enabled state, modality, and
value bounds again before invoking JUCE. Tests inspect Chrome's actual accessibility
tree, exercise keyboard slider/button actions, and reject disabled/hidden/retired
controls. Text and code editors expose native browser text fields, synchronizing
content and Unicode selections through JUCE's text interfaces. Replacements use
JUCE's insertion/undo path, compare the previous content before applying an edit,
and reject stale or read-only writes. Native browser composition stays within one
JUCE undo transaction. Focusing an accessibility control updates JUCE focus;
clicking the canvas returns to the existing EditContext path without treating
that transfer as leaving the JUCE window. Visible text targets nested in custom
controls are included even if JUCE's focus-container traversal omits them. The
original patch-search field is covered through editing, results, and patch loading.

Type-ahead fields and their result lists advertise native key navigation to the
browser bridge. Arrow, page, Home/End, Enter and Escape keys use the original JUCE
handlers, and an explicit navigation action follows the resulting native focus.
Ordinary text fields keep browser caret handling. Keyboard events marked as IME
composition remain with the browser rather than submitting or dismissing a search.
The expanded workflow verifies search cancellation, filtered-result keyboard
selection, composition confirmation, favorite menu selection and persisted favorite
removal. Both JUCE browser targets build, and 19 patch-browser/accessibility checks
pass (1.1 minutes), including the existing Unicode and composition-undo checks.

The patch-selection suite also verifies Control/Command+F opens native search
and Alt+F toggles the current favorite, with Chrome's default actions suppressed.
All eight patch-selection checks pass. The scene/oscillator shortcut regression
verifies Alt+S and Alt+1/2/3 against the original JUCE controls in both scenes,
including independent pitch edits retained when returning to each panel and
suppression of browser defaults. This covers the default canvas-focused bindings;
custom remappings and other focus contexts require separate verification.
The combined scene/oscillator, JUCE input/rendering and patch-selection run passes
all 15 checks in 1.1 minutes.

The original Keyboard Shortcut Editor supports learning a modified key chord,
canceling the edit, and saving or resetting the binding across browser reloads.
Browser modifier transitions now call JUCE's modifier-change handler without
emitting a key press: previously Learn captured Alt itself as `Alt + #12` before
the intended letter arrived. The regression presses and releases Alt, Shift,
Control and Command alone before learning, and verifies they leave Learn pending.
Alt+B opens and closes the editor.
Both browser targets build after the modifier fix. All 58 shortcut, JUCE input,
accessibility, clipboard, patch-selection and text-composition regressions pass
in the combined run (2.1 minutes).

Shortcut rows also refresh their accessible Toggle/Learn/Reset names when the
JUCE list reuses them during scrolling. Previously the Open Manual row could
announce controls for Select Oscillator 3 even though they edited Open Manual.
The scrolling regression checks each visible row before and after recycling,
toggles and resets a recycled row, then checks the rows again after scrolling
back. Additional coverage exercises conflict rejection and retry, disabled-binding
persistence, retention of the learned chord when re-enabled, and Reset All.
Virtual-keyboard layout selection is covered separately below.
The rebuilt browser target passes all 16 shortcut-editor, scene-navigation,
accessibility and popup-routing checks in the combined run (43.6 seconds).

The accessible Learn button now forwards key chords while learning is active,
without requiring focus to move to the canvas. Modifier-only transitions and
IME composition events are excluded. A browser-specific Learn button gives
learning priority over its usual Enter activation; outside learning, normal
button behavior remains. Tests cover letter, arrow, function and Enter bindings.
After learning, Enter still activates a focused native button before global
shortcuts, matching JUCE focus semantics.
Both browser targets build, and the final combined run passes 44 accessibility,
shortcut, patch-search and composition checks (2.0 minutes).

The intermittent Escape-after-reload failure was reproduced with a disabled
search field displaying `Updating patch database: 685 items left`. Disabled fields
cannot receive browser focus, so Escape never reached the type-ahead handler.
The browser editor now cancels an active search when Escape reaches the editor,
after giving open overlays their normal dismissal priority. Catalog indexing
continues independently. The regression opens/cancels search 32 times across four
page loads, records whether each field was disabled, and checks indexing finishes
without reopening a canceled search.
The browser target builds and all 16 combined shortcut/search checks pass
(2.1 minutes). A further run with an explicit disabled-field coverage assertion
passes all 32 cancellations while indexing is active (38.9 seconds).

The original virtual-keyboard menu retains all eight layouts across reloads.
Tests reject the two-octave layout while its note keys conflict with the default
octave/velocity shortcuts, retain the previous layout, then allow selection after
those four bindings are disabled. Letter-key playback needed a platform fix:
Surge stores lowercase key mappings, while browser key state uses uppercase DOM
codes. Key-state queries now normalize ASCII letters. Function keys use a separate
JUCE code range so F1-F12 cannot alias lowercase letters; canvas and accessibility
key events both translate to that range.

After restoring AZERTY, `q` starts a voice and produces nonzero real-worklet output
at 44.1/48 kHz, releasing it ends the voice, and `a`/F2 do not start voices over
32 processing blocks. Both browser targets build and all 28 layout, shortcut,
popup and text-input checks pass (1.0 minute). Broader note-position checks are
described below; physical keyboard, octave/velocity and tuning behavior need
separate verification.

Dvorak's comma/period character entries now use Chrome's punctuation key codes
for both note mapping and layout-conflict checks. Previously comma did not match
held browser keys, and period's character code overlapped Delete. Tests now play
and release both punctuation notes through the actual worklet at 44.1/48 kHz,
verify Delete/F4 remain silent, reject a conflicting period shortcut, and accept
an unrelated Delete shortcut. The browser target builds and all 15 combined
layout, shortcut-editor and scene-navigation checks pass (1.1 minutes).

Every key in all eight native layout definitions now has a real-worklet pitch
check at 44.1/48 kHz: 342 note measurements, each within 0.2% of its expected
equal-tempered frequency using Init Sine, followed by note-release verification.
The test reads the original C++ layout order rather than copying it into another
specification. Numeric OEM keys are driven with Chrome's US test-key names, so
this verifies mapping and synthesis rather than a physical keyboard/OS layout.
This exposed a missing `]` note: the desktop high-bit compatibility check treated
Chrome code 221 as code 93 (Context Menu) and excluded it. Browser layout setup
now checks the actual key code without that desktop transformation. The browser
target builds, and all 23 combined layout/pitch checks pass (2.4 minutes).

Focus transfer while a virtual-keyboard note was held exposed another browser
gap: opening patch search disabled note forwarding before the eventual key-up,
leaving the note sounding. On release with forwarding disabled (or the keyboard
hidden), the browser now invokes JUCE's keyboard-owned note cleanup instead of
rescanning text-entry keys. This avoids starting notes from held typing keys and
does not send a global panic. Six live-worklet checks pass at 44.1/48 kHz: octave
down/up changes the sounding pitch correctly; focus transfer to a browser control
or to canvas/accessible search releases the virtual note; an independently held
MIDI note survives; and playing resumes normally after search closes (51.8 seconds).
The browser target builds, and all 27 related text-input/accessibility regressions
also pass (30.3 seconds).

Octave changes while a key remains held now release keyboard-owned notes from
the old mapped range before installing the new octave. Previously an upward
change could leave the old note outside JUCE's subsequent key scan, with two
voices sounding and the old voice surviving key release. The cleanup runs only
when the octave actually changes; the original 0–9 limits remain. Tests measure
the new pitch, require a single held voice, and require zero voices after release
for both directions at 44.1/48 kHz. The browser target builds and all 15 combined
layout/performance regressions pass (1.5 minutes).

Virtual-keyboard velocity shortcuts `9`/`0` are verified at both sample rates
through measured output gain with the original VCA velocity sensitivity set to
-48 dB. Tests cover reduction, repeated lower/upper-limit presses, recovery from
zero, and note release. Native GUI velocity zero still invokes `playNote` with
zero velocity through `processBlockMidiFromGUI`; it is distinct from incoming
wire MIDI Note On velocity zero, which releases a note. The browser preserves
that behavior: the GUI note reaches the expected -48 dB minimum gain, then
recovers and clamps back to full gain. All 10 combined keyboard-performance
checks pass (1.1 minutes); no synthesis behavior was changed for this validation.

The original Alt+K virtual-keyboard toggle is verified at both sample rates:
hiding while a key is held leaves no stuck note, keys do not play while hidden,
showing restores playing, and the editor's canvas height follows the visible
keyboard. Both hidden and shown preferences survive reload. The two expanded
toggle workflows pass (11.1 seconds).

Pointer playing and note latching are verified through the real worklet at both
sample rates in `keyboard-performance.spec.js`: clicking C4, dragging to D4,
releasing outside the canvas, right-click latch/unlatch, and left-click unlatch.
The pitch checks use rendered audio. JUCE retains the last dragged note outside
the keyboard until mouse-up; the browser preserves that behavior and releases
on mouse-up. All 14 combined keyboard-performance checks pass (1.4 minutes).
The same suite now also verifies pitch-wheel bending to both two-semitone
limits, clamping when dragged beyond the wheel, and return to center on release
outside the canvas. Sustain holds a released key for at least 128 processing
blocks, then releases it when toggled off; subsequent notes release normally.
A test patch imported through browser drag-and-drop routes the modulation wheel
to oscillator pitch by one octave. Rendered pitch verifies zero, midpoint and
full depth, clamping, and retention after mouse release. All six added wheel and
sustain checks pass at 44.1/48 kHz (four in 11.6 seconds; two in 7.6 seconds).
These checks exercise the original pointer controls through Chrome and the real
audio worklet; physical controllers and assistive technology remain separate
validation requirements.

JUCE announcement requests reach persistent Chrome live regions: low and medium
priorities are polite, and high priority is assertive. Repeated messages clear and
repopulate the region; rapid pending messages of the same priority coalesce to the
latest. Text is inserted literally, never interpreted as HTML. Tests inspect both
DOM updates and Chrome's accessibility tree. Actual spoken delivery still needs
manual screen-reader verification. See [ARIA live regions](https://www.w3.org/TR/wai-aria-1.2/#aria-live).

Composition ownership uses a generation as well as component identity across DOM
and canvas editors. Tests delay the old DOM completion at the browser-to-Wasm
boundary until a newer canvas composition starts, in both the same editor and a
different editor. The late completion cannot terminate the newer composition or
split its undo transaction.

This remains partial accessibility support: complete table/text interfaces,
compound widget navigation, canvas-to-screen-reader focus tracking,
and manual assistive-technology verification remain open. Browser text editing
and accessibility-tree assertions do not establish complete screen-reader parity.

## Audio deadline profiling

With the browser target built and the local server running, capture actual
Chrome AudioWorklet callbacks with:

```sh
node web/scripts/audio-profile.mjs --rate 44100 --seconds 60 --voices 16
node web/scripts/audio-profile.mjs --rate 48000 --seconds 60 --voices 16 \
  --patch '/factory/patches_factory/Polysynths/Mega Mega.fxp'
```

The profiler uses installed desktop Chrome, warms playback for two seconds,
then samples engine voice count, audio progress, patch identity, linear Wasm
memory size, JavaScript heap usage and editor timer lateness. Output is muted
after the analyser. Set `SURGE_TEST_SILENT_OUTPUT=1` only when profiling without
a working output device. Each run creates a new directory under `web/profiles`
containing `report.json` and the original `trace.json.gz`; failures also write
`error.json`. Keep these artifacts when reviewing measurements.

Trace analysis counts complete worklet callbacks once, excludes nested author
events, and compares wall duration against the actual quantum/sample-rate
budget. Whole-graph duration is reported separately when Chrome provides the
matching thread and frame count. Missing or ambiguous callback traces fail;
inconsistent thread CPU clocks are reported instead of treated as valid timings.
Run `cd web && npx playwright test tests/audio-profile-summary.spec.js` to verify
the analysis rules.

[Historical measurements](parity/performance.json) record four 60-second runs
of Init FM2 and Mega Mega at 44.1/48 kHz, with 16 observed active voices. No
callback exceeded its quantum budget; observed maximum callback times were
474/472 microseconds for Init FM2 and 488/1022 microseconds for Mega Mega.
Linear Wasm memory did not grow. These headless, silent-device measurements are
partial evidence: they do not establish physical-device underrun behavior,
absence of internal heap leaks or audio-thread allocation, interactive editing
responsiveness, or performance of all expensive patches. Historical captures
are not bound to binary hashes and do not certify later builds.

Browser `.wtscript` saves now write a temporary sibling and rename it only after
successful close. An injected write failure previously reduced an existing file
to zero bytes; write and rename failure tests now retain the old file, preserve
the editor source, clean up temporary files, and allow a valid retry. Overwrite
cancellation also retains the file. XML/Unicode and binary snapshot save/load
checks verify persistent bytes, source and dimensions, and exact regenerated
snapshot samples. Picker handles are mocked. The binary test explicitly selects
a wavetable oscillator after reload, matching the native editor shortcut guard.
The surrounding 19 script/inventory checks and the corrected binary round-trip
check pass. Load and Save script commands now have explicit parity reviews.

Delayed `.wtscript` picker results now validate their original target before
changing oscillator state. Patch replacement, newer applied scripts and new
unapplied editor text retain their samples/code and report cancellation; a fresh
import succeeds afterward. The target snapshot and callback exclude an active
patch loader. Tests reproduced the old behavior replacing both a newer patch and
script with the late import. The updated build passes five focused XML/binary
import, persistence and delayed-picker checks. The surrounding run passed 22
checks; one save test read before completion and now waits for file existence.
Its corrected six-check rerun passed, including both overwrite failure modes.
This does not close the broader control/audio ownership audit below.

Patch changes retain the native cancellation behavior for pending script filename
and overwrite dialogs: a new file is not created and an existing file is not
modified. Both cases permit a fresh save afterward. The oscillator wavetable
menu's separate Save as .wtscript entry now has direct UI/file-content coverage
and its own parity review. These checks, script persistence, binary snapshots,
write/commit recovery and the inventory gate pass together: **11 Chrome checks
in 35 seconds**. No production change was needed for save-dialog cancellation.

Wavetable dimension menus have explicit coverage for all eight sample resolutions
and fifteen frame-count presets. Edits leave the live table unchanged until
Generate; generated dimensions and frame-dependent boundary samples are checked
for every preset. Frame-count type-in verifies the current label/value, rejects
out-of-range and nonnumeric input, cancels safely, and generates non-preset counts.
Rapidly reopening a number-field popup exposed duplicate modal menus: mouse-down
and right-button double-click both opened one. Browser number fields now suppress
the redundant double-click popup. The rebuilt application passes all three
workflow tests plus four inventory checks (**7 checks in 27.9 seconds**).

## Remaining parity work

This branch must not be described as feature complete until these pass:

- Audit and verify control/audio ownership in the JUCE AudioWorklet bridge;
  the development C ABI must only be accessed with exclusive engine ownership.
- Complete off-callback formula preparation for live edits. Browser startup and
  patch loading now prepare bounded per-modulator compiled chunks under exclusive
  engine ownership, without executing user code or consuming shared-reset requests.
  `LFOModulationSource::attackFrom()` still calls `Formula::prepareForEvaluation()`;
  a source changed after preparation takes its synchronous compilation fallback.
  The formula editor's separate display state does not prepare the audio state.
  Live-edit handoffs and first-evaluation allocation/deadline safety remain open.
- Real-device MIDI/MPE and audio input, full event scheduling coverage, sustained
  polyphony/expensive-patch profiling, and allocation audit. Basic live playback,
  patch handoff, failure recovery, and suspension have coverage at 44.1/48 kHz.
- Complete application-level file workflows, patch round trips, transactional
  import validation, scripted wavetable supersession/export workflows, and lazy loading for remaining
  non-patch assets. Patch and skin catalog/selection, browser picker adapters, and persistent
  filesystem behavior already have automated coverage.
- Physical IME and clipboard permissions, CJK/emoji font fallback, complete accessibility
  interfaces and screen-reader workflows, and complete keyboard/focus behavior. Pointer capture and high-DPI rendering have
  diagnostic coverage; advanced editor interactions still need verification.
- All desktop controls, menus, advanced editors, undo/redo, Lua extensions,
  tuning, effect families, skins, and reference audio/screenshot comparisons.
- Hide desktop-only OSC, native device settings, and host/interprocess actions.
- Static release pipeline, content hashing, asset licensing notices, and
  source distribution packaging; retire native product jobs only after parity.

## Sources and licensing

Surge retains its GPL-3.0-or-later license. JUCE 8 retains its upstream licensing
terms (including the AGPLv3 option); see `libs/JUCE/LICENSE.md`. Any distribution
must include applicable notices and corresponding source, including the pinned
submodules and browser overlay generator. Do not distribute only the generated
Wasm binary without complying with those licenses.

Browser key constants are adapted from Dreamtonics/juce_emscripten at
`3cbf82253849d66dec76d21f67ee5ff644e2e30a`; their original copyright and license
notice is retained in `BrowserKeys.h`. No older JUCE fork replaces the current
Surge JUCE version. Lua and Lua BitOp retain their upstream MIT notices in the
CMake downloaded source trees. These must be included in release packaging.

Web MIDI reference: https://www.w3.org/TR/webmidi/
Audio bridge reference: https://emscripten.org/docs/api_reference/wasm_audio_worklets.html
Threading/hosting reference: https://emscripten.org/docs/porting/pthreads.html

## Current verification evidence

The full Chrome regression suite passed **823 checks in 39.3 minutes** with
`SURGE_TEST_SILENT_OUTPUT=1 npm test` after the user-file exports, patch metadata,
default-patch persistence, tuning validation, tempo recall, and keyboard fixes.
This run precedes the subsequent Lua editor shortcut changes. It used the real
AudioWorklet graph with physical audio output disabled; hardware verification
and the remaining feature-inventory reviews are still required.

The subsequent Lua editor shortcut fix passes six focused Chrome checks:
indent/unindent and Duplicate with Command or Control, Apply to preview without
generating a wavetable, Find, Replace, and Go to Line. Browser accessibility now
forwards these commands to the original editor and names its search fields.
Go to Line also reacts to browser text changes using the original caret logic.
The surrounding accessibility, clipboard, text-composition and wavetable-script
regressions passed. Inventory checks initially detected the stale generated
snapshot after these edits; regeneration and all four inventory checks passed.
At that point the review count was **65 reviewed and 669 unreviewed**.

Lua editing now also forwards native newline indentation, tab-stop Backspace,
and directly typed delimiter pairing from the accessible editor. Selection is
synchronized before native commands so an immediately preceding browser caret
move cannot redirect the edit. All **13 Lua editing checks** pass, including all
five bracket/quote families, skipping closing delimiters, deleting empty pairs,
wrapping selected text, undo/redo, and literal IME composition. The surrounding
accessibility, clipboard, EditContext, wavetable scripting and inventory checks
also pass. An initial post-redo test expected the caret inside a restored pair;
it was corrected to preserve JUCE's existing caret-after-insertion behavior.
Five wavetable export commands now have explicit parity reviews based on file
format, sample, metadata, destination-preservation and failure-retention checks.
The inventory currently has **104 reviewed and 632 unreviewed** entries.

Alias harmonic sliders now forward browser keyboard events to their native JUCE
handler. The generic browser slider path had omitted Shift+F10 and used different
jog and Home/End behavior. Five checks pass context-menu access, native coarse/fine
steps, bounds/reset, and Sine/Triangle/Sawtooth/Square presets with every harmonic
coefficient and exact undo/redo restoration. Gesture tests respect Surge's 200 ms
undo coalescing. Four preset commands now have parity reviews; the complete Alias
editor remains open for further interaction and rendering verification.
The custom-editor toggle now delegates directly to its shared show/hide logic;
its callbacks had overwritten the correct accessible label with the opposite
action. Two added tests repeatedly close/reopen Alias through accessible press
and Enter, verify the next-action label, and retain all harmonics. All seven
Alias checks and four inventory checks pass after this fix.
A subsequent 22-case Alias run also passes shifts, positive/negative and harmonic
filters, absolute/inversion/reversal, soften, linear/cosine windows and Random.
Every deterministic command checks all coefficients and exact undo/redo; Random
checks finite bounded values and restoration of both realized arrays without
resampling. The existing harmonic-index and window conventions are retained.
Fifteen further command reviews are recorded. Pointer gestures, audio and visual
verification remain part of the broader editor audit.
Six subsequent pointer checks pass single/cross-harmonic drags, exact gesture
undo/redo, Alt-click inversion, modifier-click and double-click reset, and wheel
fine adjustment. The wheel test exposed missing browser modifier propagation
while an accessible DOM control held focus. Browser wheel events now supply their
own modifier snapshot to JUCE, sharing conversion with pointer events. Shift-wheel
uses one-tenth sensitivity and releasing Shift restores coarse adjustment. Audio
and visual parity remain open.
The rebuilt JUCE verification harness passes all 11 accessibility checks after
the shared wheel change. A further Alias workflow saves edited positive/negative
and fine-step harmonics through the original patch dialog, verifies persisted FXP
bytes after a browser reload, reloads the saved patch, and imports its FXP after
replacing the current harmonics. All 16 coefficients survive both load paths
within native XML float precision. This does not yet prove additive-mode audio
or visual parity. The combined run passes all 29 Alias workflows and four
inventory checks (33 checks total).

`tests/alias-audio.spec.js` subsequently verifies one edited additive-mode patch
at both 44.1 and 48 kHz. The original JUCE editor saves a Triangle spectrum with
an added negative second harmonic and fine-step fourth harmonic. The test checks
all 16 serialized coefficients, renders the same FXP in rebuilt native and Wasm
engines, verifies repeatability in each, and requires relative RMS error below
`1e-5`. A fundamental-only control changes just the serialized coefficients and
must materially change the signal. Both rate cases pass, using variable render
buffer sizes and note release. This covers that saved additive fixture; other
Alias modes and visual parity remain open.

`tests/alias-live.spec.js` adds two real AudioWorklet checks, at 44.1 and 48 kHz.
With a note held, setting the second harmonic changes measured spectral energy;
Undo removes it and Redo restores it without retriggering the note. A harmonic
reset while the context is suspended takes effect after resumption. Both checks
pass. The analyser graph is muted at its output, so this is not physical-device
verification. The subsequent ownership audit found direct DSP reads of the same
plain-float harmonic array that menu/gesture callbacks modified.

The initial handoff building block is `src/common/dsp/ControlSnapshot.h`: a
three-slot, single-producer/single-consumer latest-state exchange with no waits,
allocation or retry loops. `surge-control-snapshot-check` verifies superseding
unconsumed values, unchanged outputs when no update exists, coherent 64-word
payloads during one million concurrent publications, final delivery and slot
reuse. Both the native target and a ThreadSanitizer build pass.

`BrowserOscillatorExtraConfiguration` now integrates that exchange. Menu/gesture
edits, undo/paste, initialization and patch loads publish complete values under a
recursive writer lock. GUI display and file serialization read a locked copy;
DSP consumes a snapshot without taking the writer lock. Audio-thread type changes
first try the writer lock and leave the type queued when it is unavailable, so
their nested default initialization cannot wait for a UI edit. This also prevents
an older queued coefficient publication from replacing newer type defaults.
Native builds keep the plain value representation and the existing FXP fields.
The dedicated `surge-oscillator-extra-configuration-check` passes natively and
under ThreadSanitizer: incomplete transactions remain invisible, contended type
resets return immediately, and 100,000 UI transactions can interleave with type
resets without torn snapshots. This is a focused harmonic-state handoff audit,
not a claim that every synth parameter/editor ownership issue is closed.
After integration, 114 Alias/editor/audio, factory-script, binary-script, import,
export and snapshot regressions pass. Two additional Alias copy/type-reset cases
pass with audio running and inactive; eight patch-validation/navigation checks
also pass, including native preflight of all 3,561 factory patches. The transfer
test follows the native Classic submenu through its Sawtooth selection before
switching back to Alias and checking all default coefficients.

Oscillator-level undo now captures and restores extra configuration as well as
parameters and modulation. Paste undo previously reset an edited Alias harmonic
to its initialized value (for example, harmonic 4 changed from `-1` to `0.25`).
The transfer regression now checks every harmonic across paste Undo/Redo and
Alias-to-Classic type-change Undo/Redo, with audio both running and inactive.
It reopens the custom editor after undo rebuilds the oscillator panel, matching
the existing UI workflow. Paste can trigger a second queued panel rebuild; the
coefficient test reopens a retired view without changing its data. Editor-view
retention across such refreshes remains outside this state-restoration check.
Both transfer cases pass three consecutive runs after that synchronization
adjustment; the other 35 Alias editor/live-audio and inventory checks also pass
against the rebuilt undo implementation.

The follow-up native source audit confirms that `onOscillatorTypeChanged()` is
called for general editor refreshes and closes non-wavetable custom editors on
desktop as well. The browser test's reopening follows that existing behavior.

`tests/oscillator-wavetable-undo.spec.js` reproduced a separate defect: pasting
Sine over Triangle and choosing Undo retained the Sine name and samples.
`UndoOscillator` now captures the complete table, script source/dimensions and
both captured inputs, and accounts for those buffers in its memory limit.
Control-side copy/paste and oscillator undo use shared storage helpers: browser
replacements are prepared privately and swapped at an audio-block boundary,
with old buffers retired on the control side. A pending control replacement is
the logical source for a later copy/undo; unfinished script-generation results
are not treated as committed control edits. Native builds replace synchronously.

All four factory/scripted-table cases pass with audio running and inactive. They
compare every sample through Undo/Redo, verify suspension/resumption, retain an
independent source oscillator, and verify restored Lua source. Regeneration from
both restored captured inputs checks their samples and the saved generation
dimensions. A supporting 17-case run passes Alias transfer, scene-paste/FX,
patch validation and inventory checks. Both native and Wasm builds pass. This
does not close the broader callback-ownership or deadline-performance audit.

Wavetable-selection undo also captures exact table buffers for factory entries,
including resliced dimensions, and restores them through the same prepared-data
handoff. It no longer reloads a factory file or copies directly into a live DSP
table. The two selection-undo cases in `tests/wavetables.spec.js` pass with audio
inactive and running: they remove the original factory file, deny subsequent
asset downloads, and compare every sample and dimension through Undo/Redo.
The focused 12-case run also passes scripted-selection and oscillator-paste
regressions. These automated checks use Chrome's real AudioWorklet with silent
output; they do not verify physical audio hardware.

Scene paste exposed a separate whole-patch undo issue: undo serialization obeyed
the optional snapshot-export setting, so Undo/Redo lost captured Lua inputs even
though it retained the generated table and script. Undo serialization now always
includes working snapshots and records the original export preference separately.
It also streams otherwise-clean patches when they contain captured inputs, rather
than relying on their factory file. Ordinary patch exports retain their existing
opt-in behavior. `tests/scene-wavetable-undo.spec.js` checks original scene-menu
copy/paste, exact table restoration, the unchanged export checkbox, and subsequent
generation from both copied inputs with audio inactive and running.
Both cases pass after reproducing the missing-input failure before the fix.
The supporting 27-case run passes oscillator undo, live scene/FX operations at
44.1/48 kHz, patch selection/validation and inventory checks. Native reference
and WebAssembly application builds also pass.
Two additional persistence cases pass with snapshot export enabled and disabled.
They compare persisted FXP bytes after browser reload, restore the exact table
and Lua source, and verify that captured inputs are included only when requested.
The enabled case regenerates all 384 expected samples from both restored inputs
and confirms that the export checkbox remains enabled.

Wavetable-selection undo now also captures both Lua input slots, invalidates
cached snapshot bundles, and counts their buffers against the undo memory limit.
Previously, selecting a factory script after importing a binary snapshot script
cleared its inputs; Undo restored the source and generated table but regeneration
failed. `tests/binary-script.spec.js` exercises that sequence with audio running
and inactive, including Redo clearing the inputs again, another Undo, exact sample
regeneration, binary export and persisted reload.
Both cases pass after reproducing the failure before the fix. The rebuilt
WebAssembly app passes the focused 10-case selection/scene/persistence run;
the inventory remains at 104 reviewed and 632 unreviewed entry points.





Factory wavetable script items now use JUCE's standard browser menu action path.
Their previous custom renderer invoked loading only from mouseDown, so accessible
menu activation closed the menu without loading a script. All 13 bundled scripts
now pass original-menu selection, source-content, generated-dimension, and finite
non-silent sample checks. Source comparisons normalize editor line endings.
Tests wait for generated dimensions because the display name updates before the
worker publishes its result. HQ scripts took up to about 40 seconds in this run;
this is functional coverage, not a performance-parity claim. Two additional
checks pass Enter activation, checked-item reselection, and nested user-category
selection after saving and reloading browser storage. The latter verifies every
generated sample. Checked category rows use their native checkbox accessibility
role. Both dynamic menu paths now have explicit parity reviews. The standard
item adds one discovered inventory entry (735 total).

The oscillator wavetable browser had the same custom-item accessibility gap for
scripted entries: pointer and keyboard activation worked, while accessible press
closed the menu without loading the selected script. Browser scripted entries
now use standard JUCE items with the original icons and action. Three input-path
checks verify source and generated dimensions; five wavetable loading, failure,
reslicing and stale-result regressions also pass. The pointer test respects JUCE's
250 ms standard-menu opening guard. This adds another discovered entry (736 total);
the wider oscillator menu review remains open. Two additional tests verify exact
sample, name, dimension and source restoration through undo/redo from both script
selection entry points. The saved nested user script also reloads through the
oscillator menu and generates exact samples. Its scripted-item action now has an
explicit parity review; other oscillator-menu actions remain separately tracked.

The oscillator file picker now holds a safe component reference and checks the
patch generation, oscillator binding/type, queued selection and publication token
before accepting its result. A reproduced stale picker previously replaced a
newer Triangle selection with Imported; it now reports cancellation and retains
the newer selection. Direct WT import verifies every decoded sample, and five
wavetable loading/reslicing/failure regressions pass. The file-import menu review
now has an explicit parity review covering the import workflows and failure cases
below. Picker handles and permission outcomes are simulated in these tests.
A subsequent seven-case run verifies WT, tagged PCM WAV, 32-bit float WAV and
XML script imports with exact samples/dimensions, plus stale selection, scene and
patch cancellation. Tagged PCM fixtures retain Surge's native 16384 half-range
wavetable scaling; float WAV and script fixtures retain their exact amplitudes.
The guard now also checks applied Lua source, dimensions, snapshot version and
editor text. WT and script pickers reject results after new drafts or Apply,
including editors opened while the picker was pending. An already open editor
allows the import requested for its unchanged draft but preserves later edits;
simply opening an unedited editor also permits the import. Fourteen import
workflow tests and four inventory checks pass after this change.
Five additional checks verify invalid WT/WAV/XML-script retention, picker
cancellation and permission denial; each preserves current samples/source where
applicable and permits a fresh successful import. This brings oscillator file
import coverage to 19 workflow tests.






Wavetable snapshot rows now expose browser-accessible Capture and Load actions
without changing the painted layout. Both snapshot slots pass live oscillator
capture, exact-sample script regeneration, and clear workflows. Invalid snapshot
imports report failure and retain the previous snapshot; regeneration verifies
that its samples survive. All 28 wavetable scripting and binary-script checks
pass after these changes. Snapshot picker callbacks now use a safe editor reference
and reject stale results after patch replacement, changed snapshot inputs, newer
generation, or editor text edits. Six focused Chrome checks verify exact WT
import samples and these delayed-result cases, including a closed editor. The
snapshot submenu now has an explicit parity review: a subsequent six-case run
verifies capture from every oscillator in both scenes. Those tests wait for source
publication before opening the menu, so asynchronous UI rebuilds cannot cancel
the test interaction. These tests use mocked file-picker handles. The preceding
38-check scripting and inventory run also passes WAV snapshot import and
exact-sample regeneration.


Five Lua context-menu commands now have explicit parity reviews: Find, Replace,
Go to Line, and read-only Copy/Select All. Native pointer events open the menus;
checks verify selection positions, Unicode replacement, complete prelude copying,
and omission of editing actions from the read-only prelude. The prelude also
supports menu-based search and navigation without changing its text. Clipboard
writes are mocked. A test initially clicked a closing popup after Copy; waiting
for menu dismissal fixes that test race. The combined editor and inventory run
passes **27 Chrome checks in 1.2 minutes**.

Six additional formula-editor tests pass in Chrome: Unicode code survives Apply,
patch undo/redo and a saved FXP reload; debugger Init/Step and filtering work;
runtime, syntax and invalid-init errors retain editable code and recover after
correction; close confirmation preserves or discards unapplied edits as selected;
and the read-only prelude remains searchable while rejecting edits. Real worklet
tests at 44.1/48 kHz route formula output to sine-oscillator pitch, wait over 64
additional audio blocks after each edit, and verify that held voices retain the
old function while newly triggered notes use the edited function. These tests
use silent output and do not resolve the first-use compilation issue above.

The formula ownership audit found that the separate display and audio Lua
interpreters shared mutable C++ function-cache containers. ThreadSanitizer
reproduced races in those containers. Each interpreter now owns its own cache;
shared-table reset requests are consumed with an atomic exchange, and duplicate
console writes during preparation were removed while retaining the normal error
details. The concurrent native fixture and focused ThreadSanitizer check pass
without sanitizer suppressions. This does not resolve compilation on note attack.

Eight native/WASM audio comparisons also pass at 44.1/48 kHz for formula phase
math, BitOp counters, the second vector output, and shared initialization. Each
fixture repeats exactly in the native harness, audibly differs from its unrouted
control, and matches WASM within relative RMS error `1e-5`. The rebuilt browser
passes formula editing, debugger error recovery, live pitch changes, and scripted
wavetable workflows. Two Lua editor tests initially called an export before WASM
initialization; waiting for the native UI corrected their readiness check. The
combined audio/context/Lua editing rerun passed **22 checks in 42 seconds**.

Formula compilation and execution are now separate operations. Browser patch
loading and audio startup retain compiled chunks in bounded per-modulator slots
while the engine is exclusively owned; script bodies still execute when the
original evaluator would execute them. Syntax failures retain their diagnostics,
and compiling a replacement neither resets shared tables nor replaces held-voice
functions. Superseded chunks are released, and storage destruction now closes
both Lua interpreters. Native checks cover these lifetime and timing contracts.
The eight formula audio comparisons also assert that patch preparation compiled
the scripts and that note attack/rendering perform no additional compilation.
Edits after preparation still take the synchronous fallback and require a live
audio handoff before the remaining real-time requirement can be closed.
The measured live-edit result is recorded in
[`parity/formula-live-compilation.json`](parity/formula-live-compilation.json).
An atomic diagnostic counter permits observation without touching the audio Lua
interpreter from the UI thread. At both 44.1 and 48 kHz, two edits preserve held
notes and change new-note pitch, but each next note attack increments the source
compilation count once. The two passing sound-continuity tests therefore do not
close the off-callback compilation gate. Their Playwright attachments retain the
before-Apply, before-attack and after-attack counts. The native deferred-execution
and compilation-lifetime check also passes with the atomic counter.
After this change, both native and browser builds pass, as do the focused
ThreadSanitizer check and **61 Chrome checks in 3.2 minutes** covering formula
audio/editing, Lua editing, wavetable scripting/export, audio lifecycle, and patch
selection. Chrome used silent output; physical-device verification remains open.

Before the active Airwindows parameter expansion, on September 27, 2026, the
full suite passed **422 checks in 11.9 minutes** in
desktop Chrome 153.0.8010.53, using
`SURGE_TEST_SILENT_OUTPUT=1 npm test --prefix web`. This includes the search retry
correction, expanded Airwindows comparisons and isolated-effect checks alongside
the existing editor, AudioWorklet, import/export, persistence and failure-recovery
checks. Silent output runs the real audio graph but does not verify physical
audio hardware. The source inventory and whitespace checks pass; the completion
gate still fails with **664 of 734 entry points unreviewed**. Full portable-feature
parity, unresolved audio cases, hardware checks and sustained performance remain
requirements. Earlier runs below document the changes leading to this result.

An earlier full run, before the Ensemble timing correction, passed **all 266 checks**
in desktop Chrome with the pinned
Emscripten 6.0.10 browser build and `SURGE_TEST_SILENT_OUTPUT=1`. This includes
native snapshot-format and native/Wasm audio comparisons, real AudioWorklet
playback, MIDI/input adapters, import/export, persistence, accessibility, factory
presets, all 11 factory skin selections and exact bundle bytes, download/write
failure recovery, folder/ZIP validation and rollback, and custom-font rendering.
It also covers scene/reorder FX locking, bounded effect retirement, all 62 native
family/rate parameter-attachment cases, all-family live effect construction, and
control processing while every animation frame is held.

Single-preset and whole-chain missing/corrupt response checks pass with inactive
audio and at 44.1/48 kHz. They verify retained parameters, preset labels and undo
history after failure, successful retry, and subsequent undo. The chain fixture
puts the failing file in its fourth slot to detect partial earlier publication.
Native and browser builds, source inventory and whitespace checks also pass.
This suite does not establish full portable-feature parity or physical-device
validation; the remaining inventory and real-time limitations still apply.
After the Ensemble correction, the refreshed all-family diagnostic measured 62
family/rate cases. **67 targeted checks passed**: 65 parity/serialization checks
and two all-family live-construction checks. The parity checks include all seven
Ensemble modes at both rates, derive mode IDs from the C++ enums, reject invalid
parameter edits, and require non-default modes to change the default audio.
The later parameter/input fixtures initially produced **79 passing checks and two expected
Vocoder comparison failures**. The Vocoder correction below removes those
expected-failure annotations; all eight modulator-mode/rate checks now pass.
This includes six EQ/Exciter settings and the Audio Input level edit at both
rates. Both harness builds, the source inventory check and whitespace validation
passed. Scoped runs do not establish physical audio-device behavior.

The subsequent full 304-check run had **303 passes and one search-result retry
failure**; all audio comparisons passed. The trace showed no download attempt and
an empty result list. Review found that the database's outstanding-job count
excluded its active transaction batch. That count now includes active work until
the transaction finishes (or returns ownership to the queue). Search interaction
checks also wait for the cleared field, completed clipboard paste and unique
matching result instead of a fixed delay or a stale accessibility row. On the
final builds, **10 repeated search checks and 29 focused clipboard, patch-selection
and Vocoder checks passed**. The complete 304-check suite has not been rerun after
this final readiness correction. Both builds, the 10,510,706-value native envelope
check, source inventory and whitespace validation pass.

An initial full-suite attempt stalled in Node's synchronous archive-fixture
process: Python was waiting for stdin EOF while Node waited for the child.
That run was stopped after process samples identified the fixture handoff.
ZIP fixture generation now uses temporary input/output files, a 10-second child
timeout and guaranteed cleanup. All 16 archive/skin-import checks passed on their
own before the successful 222-check full run.

An earlier rebinding regression run had one initial-patch timeout: downloads
succeeded but a later startup asset request followed an approximately 84-second
gap. That case then passed three unchanged reruns. Control processing now runs
independently of animation frames, with bounded runtime/handoff diagnostics
attached to browser-test failures. The full run above did not reproduce the
stall. The paused-frame test proves independence from repaint scheduling, but
does not establish that this was the cause of the earlier 84-second gap.

Live convolution checks at 44.1/48 kHz now cover initial IR loading during
playback, repeated IR switching, size/start edits, clearing and reloading the
response, effect copy/paste, replacement with a different effect and a patch
change. The native worker check covers bounded reuse, matching sample ownership
and settings, stale/failure retention, standalone kernel retirement, cancellation
without a remaining consumer, and sample-buffer destruction on the worker.
These checks do not complete the real-time allocation/deadline audit or the
remaining FX writer concurrency work described above.

Native captures at 44.1/48 kHz were byte-identical before and after kernel
isolation. Native/Wasm stereo-convolution relative RMS error passes the `1e-5`
threshold at both rates; rejected non-finite IR input retains the preceding
response. With host output, the earlier live-worklet run failed because Chrome's
audio clock stalled; an independent plain oscillator reproduced the stall.
Silent output retains real worklet execution, but physical output remains
unverified for this run.

The copy/paste workflow exposed a cached accessibility menu: unlike mouse
activation, it did not refresh after Copy. Accessibility activation now rebuilds
oscillator and FX menus before displaying them, matching mouse behavior. The full
suite above includes the resulting live FX copy/paste and wavetable regressions.

The browser target rebuilt successfully, `git diff --check` passes, and the
refreshed inventory audit passes. The current inventory has **734 entry points, 70
reviewed and 664 unreviewed**. The skin-selection review covers delivery and
selection workflows; native/browser visual parity at every zoom/display scale
remains separate work. Passing regression checks does not complete the
portable-feature inventory.

- Emscripten builds the JUCE platform diagnostic, complete Surge editor target,
  and engine AudioWorklet module.
- Stable Chrome 153.0.8010.53 renders the original embedded Surge skin, fonts,
  and editor. Earlier intermittent startup failures were traced to main-script
  download errors before Wasm initialization. The development server now reuses
  HTTP connections; bootstrap failures show a download error and support reload
  recovery. The suite covers: JUCE mouse callbacks, high-DPI
  coordinates, pointer capture outside the canvas, original-editor scene switching,
  FM2 comparisons at 44.1/48 kHz, an FXP parameter round trip, imported-file
  persistence, export, picker cancellation, denied permissions, failed export,
  and storage retry. Picker handles are mocked in these automation tests;
  actual system dialogs and device permissions still require manual checks.
  Six additional asset tests verify complete byte-for-byte library coverage, lazy
  fetch/cache reuse, corrupt downloads, corrupt cache recovery, cache quota
  warnings, and unavailable downloads. Five patch integration tests cover the
  full catalog, failed download and retry, stale-selection rejection, native
  metadata search, and selection through the original JUCE category menu. Three
  validation tests cover all shipped FXPs, malformed-file rejection with identical
  before/after patch serialization, and rejection before JUCE receives an import.
  Imported patch naming now uses the filename stem during playback as it already
  did with audio inactive, rather than including `/user/imports/<id>/` and the
  extension. Two regression cases drop a renamed file whose embedded XML name
  differs, covering both loader paths. The combined 25 patch-validation and
  keyboard-performance regressions pass (1.8 minutes).
- Fifteen clipboard/search checks cover text and code editor shortcuts, context
  menus, Unicode and large scripts, undo/redo, denied access/retry, stale results,
  read-only changes, native patch search, and failed search-result download/retry.
  Clipboard permissions/data are mocked; compiled JUCE widgets and engine patch
  loading are exercised. Catalog paths from search/history/browser now enter the
  same download queue as category-menu selections.
- Sixteen EditContext checks cover real Chrome text/composition events, Unicode
  offset conversion, one-step composition undo/redo, read-only focus changes,
  cancellation, candidate bounds, and composition underlines in JUCE text and
  code editors. Physical IME candidate windows and missing-glyph fallback remain.
- Two bootstrap checks cover failed application/helper downloads and reload recovery.
- Three report checks cover sandboxing, focus restoration, exact HTML download,
  failed-download retention, and the original JUCE all-parameter patch export.
  Two inventory checks detect source drift and reject incomplete review ledgers.
- Nine audio-lifecycle checks cover timeout/retry, late completion, confirmed
  closure before replacement, cleanup failure, suspension/resumption, stale
  import rejection, bootstrap-port cleanup, actual Chrome context closure, and
  continued patch browsing during a pending worklet import.
- Eight audio-input checks cover permission retry, device replacement, late
  permission responses, disconnect/page-exit cleanup, deferred graph readiness,
  saved-device restoration without capture, and factory stereo-input processing
  at 44.1/48 kHz. Two further checks exercise real Chrome capture with a
  simulated stereo device through the live worklet at both sample rates.
- Transport tests cover concurrent state changes, sample-based advancement,
  pause/rewind through the JUCE processor at 44.1/48 kHz, invalid settings,
  and scheduled MIDI synthesis/panic through the shared offline render path.
- Eight MIDI checks cover the concurrent native queue, permission retry, MPE
  message forwarding, device selection/disconnect, device-open failure, saved
  choices after reload, failed-save retry, and invalid preference recovery.
- Wavetable checks cover catalog completeness, exact sample data, failed and
  invalid downloads, stale requests, reslicing, and patch-change invalidation.
- JUCE timers run on the browser event loop. A pthread clock conversion fix
  prevents Wasm saturation from freezing the timer counter. Browser hit-testing
  respects overlapping popup canvases, and native popups stay above browser
  transport and MIDI controls. Regression checks
  verify callback delivery and original-editor redraws after patch changes.
- FM2 comparisons cover dry output, a deterministic stereo convolution IR,
  and 23 other default-effect fixtures listed in `web/parity/effects.json`. Each renders
  three seconds at 44.1/48 kHz with varying buffer lengths. The new effect
  fixtures must differ from dry output, reject invalid effect edits without
  changing output, and retain relative native/Wasm RMS error below `1e-5`.
  These fixtures do not establish parity for every synthesis/effect family.
- Delay at 44.1 kHz initially differed by relative RMS 0.010266. The native
  Apple ARM64 compiler fuses the one-pole lag recurrence; separate Wasm
  operations drifted from a steady 11019-sample target to 11018.6787, changing
  delay interpolation phase. A digest-checked build-local SST header overlay
  uses explicit `std::fma` for this recurrence. Upstream files and the native
  reference arithmetic are unchanged. All 11 focused comparison checks and
  the subsequent 228-check full run pass at the original tolerance. Sustained
  performance profiling must include the explicit floating-point operation.
- Resonator's initial repeatable relative RMS errors were `6.37e-5` at 44.1 kHz
  and `1.12e-4` at 48 kHz. Native ARM64 assembly confirmed fused multiply-adds
  in the pitch-to-frequency table interpolation and filter-coefficient smoothing.
  The browser now explicitly preserves those rounding operations: interpolation
  in `SurgeStorage` and a digest-checked build-local SST filter-header overlay.
  Errors are now `5.27e-6` and `6.46e-6`, below the unchanged `1e-5` threshold.
  The native capture before/after rebuilding was byte-identical. The refreshed
  62-case survey records the remaining gaps, and all 266 automated checks pass.
  Stochastic Tape comparisons remain unresolved.
- Ensemble's default 48 kHz fixture initially differed by `1.01e-4` relative
  RMS. Explicitly fusing the two modulation terms before adding the center delay
  matches the native ARM64 delay-time calculation and avoids different BBD clock
  steps. Default-fixture errors are now `1.83e-7` at 44.1 kHz and `1.76e-7` at
  48 kHz. The rebuilt native 48 kHz capture is byte-identical to its prior capture.
  Digital delay and every BBD stage count pass at both rates with the unchanged
  `1e-5` threshold. Other Ensemble parameter combinations still need coverage.
- Vocoder's external-input comparison initially differed by `2.58e-3` at 44.1 kHz
  and `4.54e-4` at 48 kHz. A temporary precise-square-root native diagnostic
  isolated the native NEON envelope approximation as the main source. The Wasm
  envelope now reproduces that approximation with a 2 KiB constant lookup and
  exponent adjustment, without per-sample allocation. The normal native code is
  unchanged; before/after native captures are byte-identical at both rates.
  Errors are now `2.19e-6` and `1.13e-6`, below the original `1e-5` threshold.
  All four modulator modes pass at both rates. The ARM64 development target
  `surge-vocoder-envelope-check` verifies 10,510,706 values, including every
  positive subnormal and normal-exponent bucket endpoints; `--print-table`
  regenerates its constants from the pinned SIMDe implementation. This preserves
  the ARM64 reference approximation; independent x86-reference comparisons and
  sustained worklet performance profiling remain required.
- Real AudioWorklet playback passes at 44.1/48 kHz, including note output, panic,
  patch switching, failed downloads, suspension/resumption, processor-error
  handling, and patch loading after context replacement. Sustained deadline and
  memory profiling, arbitrary DSP crash recovery, and the full real-time audit
  remain open.
