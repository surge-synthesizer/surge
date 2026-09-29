# Browser parity inventory

`source-inventory.json` indexes the original application and JUCE standalone UI:
item/menu construction call sites, keyboard action enums, skin connectors,
non-parameter skin actions, overlay tags, and editor implementations. It retains
source locations and source digests. It includes native-only branches so that
removing a browser menu cannot make its original requirement disappear.

This is an index of entry points, **not a count of distinct features**. A call
site may generate many choices or controls. A source declaration may be reachable
from several workflows. Dynamic labels, parameter families, preset lists, and
helper-generated menu options require runtime expansion during review. The
inventory is not, by itself, a complete proof of desktop/browser equivalence.

## Review and gate

```
python3 web/scripts/feature-inventory.py --write
python3 web/scripts/feature-inventory.py
python3 web/scripts/feature-inventory.py --require-complete
```

The default check detects source/index drift and stale or invalid reviews.
`--require-complete` must fail while any entry is unreviewed. Do not weaken this
gate to make a release pass. Update `reviews.json` only after inspecting the
browser behavior and evidence for that entry. A verified review requires a
specific browser equivalent, existing evidence files, and the current source
digest. Source changes invalidate reviews in that source file.

A platform limitation requires a reason, a portable alternative, and one of the
explicitly excluded capability types: native drivers, plugin hosting, OS shell
integration, MTS-ESP interprocess integration, or direct UDP OSC. Difficulty,
permission denial, a failed test, or a missing implementation is not a platform
limitation. Such entries stay unreviewed.

The gate checks review bookkeeping, not whether a cited test proves a claim or
has passed on the current revision. A release audit must inspect and run the
evidence, expand dynamic choices, and verify the complete workflows below.
Compilation of an editor does not count as editor verification.

The latest complete automated browser run passed **970 tests in 52.5 minutes**
on stable Chrome 153.0.8010.53. [`browser-regression.json`](browser-regression.json)
records the command, result and 83 test/binary hashes, all unchanged at completion.
This run used `SURGE_TEST_SILENT_OUTPUT=1` and predates the Tri-pole fix in the
application binaries. It does not verify physical audio/MIDI devices, deadline
performance or the unreviewed source entry points (632 at that run; now 623).

## Required workflow coverage beyond the source index

| Area | Required evidence before completion | Current gap |
| --- | --- | --- |
| DSP and file compatibility | Native/Wasm comparisons across oscillator, filter, effect, modulation, tuning, and scripting families; patch round trips | All 12 oscillator families have template/default comparisons at both rates, using controlled seeds for S&H Noise and String, alongside the effect and MSEG fixtures below; parameter modes and broader family coverage remain |
| Real-time processing | Actual Chrome worklet playback, callback ownership/allocation audit, patch/edit handoffs, suspension/resumption | Real note playback, patch/download handoffs, suspension and error-event recovery pass at 44.1/48 kHz; full ownership/allocation audit and editor handoffs remain |
| Performance | Sustained polyphony and expensive patches at 44.1/48 kHz; deadline misses, memory growth, UI response | [Four historical 60-second live-worklet captures](performance.json) cover two patches and both rates; hardware deadlines, allocation/leak audit, interactive editing and broader expensive workloads remain |
| Performance input | Keyboard, real MIDI/MPE, MIDI learn, audio devices, and transport | Queue/adapters/transport have tests; real devices and full keyboard/MIDI-learn workflows remain |
| Parameter workflows | Both scenes, all oscillator/filter/effect configurations, routing and modulation, context menus | Existing JUCE implementations are compiled; broad workflow verification remains |
| Advanced editors | MSEG, step, formula, wavetable scripting, tuning, visualizers, error reporting, and undo/redo | Browser workflows and script-publication safety remain unverified |
| Interface | Original layout, skins, fonts, zoom, shortcuts, focus, accessibility, IME, clipboard | Basic paint/input/scene/patch, clipboard and EditContext checks exist; real clipboard/IME checks, CJK/emoji font fallback and accessibility remain |
| User content | Patch, wavetable, tuning and skin import/export; reload persistence; failed imports/storage/downloads preserve edits | Several adapter and patch/wavetable checks pass; all application-level formats/workflows remain |
| Factory content | Every asset discoverable and usable on demand with cache recovery | Full publication is verified; non-patch/wavetable consumers still need integration |
| Preferences | Persistent portable settings and faithful browser equivalents for relevant standalone settings | Storage exists; full setting-by-setting verification remains |
| Platform exclusions | All native-only actions hidden, with portable underlying capabilities retained | OSC settings and selected OS folder/reveal actions are hidden; remaining actions require review |
| Static delivery | Browser-only supported product, HTTPS/isolation hosting, versioned assets, CI/release jobs | Native jobs retained until parity; browser release pipeline remains |
| Distribution | Complete dependency notices, licenses and reproducible corresponding-source distribution | Packaging remains |
| Reference evidence | Native audio and workflow screenshots captured before retiring native products | Native FM2 reference exists; desktop workflow capture remains |

Examples of portable items that must **not** be hidden merely because their old
implementation opened a folder: installing a skin and accessing factory tuning
content. These need browser equivalents and remain unreviewed until implemented
and verified.

## Desktop reference build

The current checkout also builds a development-only macOS desktop reference:

```sh
cmake -S . -B buildmac -DENABLE_LTO=OFF -DSURGE_BUILD_TESTRUNNER=OFF \
  -DSURGE_SKIP_WERROR=ON -DSURGE_BUILD_FX=ON -DSURGE_BUILD_CLAP=OFF \
  -DSURGE_COPY_TO_PRODUCTS=OFF -DSURGE_COPY_AFTER_BUILD=OFF -DSURGE_SKIP_VST3=ON
cmake --build buildmac --target surge-xt_Standalone -j2
```

This build passed and produced version 1.4.0 in
`buildmac/src/surge-xt/surge-xt_artefacts/Release/Standalone/Surge XT.app`.
The Effects target is configured because the existing plugin-validation CMake
assumes it exists; only the synth standalone target was built. No system install
or product-directory copy is enabled. This is reference evidence preparation,
not a retained supported native product. Desktop screenshots remain missing:
the UI tool could not acquire the installed 1.3.4 app's window and timed out
twice when opening the newly built reference app. Compilation does not prove
desktop workflow or visual parity.

## Filter coverage inventory

The filter-family coverage gap is now indexed in
[`filters-inventory.json`](filters-inventory.json). Regenerate it with
`node web/scripts/filter-inventory.mjs web/parity/filters-inventory.json`.
It derives all 36 type IDs and names and the 204 public type/subtype combinations
from `sst-filters` declarations; these match a compiled native header probe.
The two-rate matrix has 408 cases, including the Off control. The inventory
itself makes no verification claim. It does not cover internal masks, morph parameters,
cutoff/resonance ranges, routing, modulation or live edits, and does not replace
the required native/Wasm audio comparisons.

`web/scripts/filter-fixtures.mjs` generates compatible FXP fixtures for every
public mode, using the existing isolated oscillator template with Serial 1
routing, filter 2 and waveshaper off, no filter-envelope/keytracking modulation,
cutoff 3 and resonance 0.35. Render with the Audio Input oscillator and the
harness's deterministic stereo input. All 204 payload lengths were checked.
A native LP 12 dB/standard probe at 48 kHz repeats exactly, is silent when muted,
and materially differs from bypass (filtered energy 19.4942; squared difference
25.5649). This validates that fixture's signal path, not the remaining modes or
browser parity; their comparison measurements are still required.

Run `node web/scripts/filter-survey.mjs OUTPUT.json` for the complete two-rate
matrix, or select a diagnostic subset with `--type ID --rate 48000`. The runner
checks served/local Wasm hashes, loaded browser filter/routing fields, exact
native and Wasm repeatability, mute silence and a material bypass difference.
Loaded cutoff, resonance, envelope/keytracking amounts and activation are also
checked; missing fields fail rather than being interpreted as zero. Each browser
render has a 30-second diagnostic deadline, after which its page is closed and
the failed case is recorded.
It records numerical discrepancies and fixture failures without relaxing the
existing `1e-5` relative RMS threshold. `complete` means all 408 cases were
measured, not that they passed; `selectionComplete` also allows a measured subset.
Binary changes during capture prevent either completion flag from being set.

[`filters-baseline.json`](filters-baseline.json) records the completed 408-case
survey of the unchanged baseline binaries: 384 cases pass, with maximum accepted
relative RMS below `8.9e-7`. Its only 24 numerical discrepancies are Tri-pole's
12 modes at both rates. The corrected Tri-pole report below is separate and has
its own binary hashes; do not describe these as one capture of a single binary.

The current survey exposed Tri-pole differences at 44.1 kHz in all 12 modes.
An isolated kernel comparison traced these to SIMDe's ARM64 reciprocal-square-root
estimate: Wasm uses exact division/square root at the same call site. Replacing
only that operation in the native diagnostic makes its output identical to the
original Wasm kernel. The browser overlay now preserves the reference estimate
specifically in Tri-pole, leaving the native implementation and other filters
unchanged. The separate corrected engine passes all 24 full-synth cases at both
rates in [`filters-tripole.json`](filters-tripole.json): relative RMS is below
`1.1e-9`, repeat renders are identical, muted renders are silent, and every
active mode differs materially from bypass. This report records the independently
built engine hash; the 408-case baseline survey remains separate.
The subsequently rebuilt primary `build-web/web` engine and JavaScript match
that independently tested build byte for byte.

After configuring the Wasm build, run the following on the ARM64 reference host:

```sh
python3 web/scripts/check-tripole-kernel.py
```

The check compares the portable estimate to actual native NEON for 10,510,706
inputs, including all positive subnormals, normal-exponent bucket boundaries and
two million pseudo-random patterns. It also compares all 12 Tri-pole kernels with
four independent lanes, fixed coefficients and inputs: all 960,000 output samples
match bit for bit using the generated CMake overlay. This isolates the numerical
operation; it does not establish full-engine, parameter-range or deadline parity.
`python3 web/scripts/check-tripole-kernel.py --print-table` regenerates the constants.

The rebuilt JUCE application also passes both `tripole-live.spec.js` cases. Each
sample rate plays all 12 imported modes through the real worklet, checks finite
non-silent output after first draining the previous note, and exercises patch
handoffs and suspend/resume. Together with the six JUCE paint/input checks,
these eight post-fix application checks passed in 38.1 seconds with silent host
output. They establish playback behavior, not physical-device or deadline proof.

Initial 48 kHz measurements in `filters-bypass-sample.json` and
`filters-lowpass-sample.json` pass for Off and all three LP 12 dB subtypes, with
relative RMS below `5e-10`, exact repeatability and zero muted energy on both
platforms. Every active subtype materially differs from bypass. These four
cases validated the comparison runner before the full baseline survey. The
broader parameter and workflow requirements above remain open.

The first full survey detected that the revision-26 template remapped BP 12 dB
subtypes 1/2 to legacy modes (and would similarly remap OB-Xd 24 dB). The loader
verification rejected these cases before comparing the wrong modes. Current-mode
fixtures now derive `ff_revision` from `SurgeStorage.h`; historical migration
behavior is a separate requirement. All five BP 12 dB subtypes subsequently
preserved their requested selections and matched at 44.1 kHz, including mute,
bypass and repeatability controls. The interrupted old-revision survey is not
completion evidence; the full matrix must be measured with the corrected fixtures.

## Oscillator audio fixtures

`oscillators.json` records all 12 original oscillator IDs and names, derived from
`SurgeStorage.h`, at 44.1 and 48 kHz. Generate a diagnostic survey with
`node web/scripts/oscillator-survey.mjs OUTPUT.json` after building both rendering
harnesses and starting the local development server. The recorded binary hashes
refer to the local harness and `build-web/web/surge-web.wasm`; the server must
serve that checkout. The survey rejects binary changes during its capture.

The fixture uses the original Init Wavetable patch with its embedded table data,
scene A oscillator 1 isolated, drift disabled and retrigger enabled. Selecting a
different family runs Surge's original oscillator-default path; Wavetable keeps
the original template settings. Audio Input receives identical stereo input on
both platforms. The browser exports the edited patch to verify the requested
oscillator type and retrigger setting before rendering. Invalid scene, slot and
type edits must be rejected without replacing that selection.

Ten families have repeatable matching fixtures at both rates: Classic, Sine,
Wavetable, Audio Input, FM3, FM2, Window, Modern, Twist and Alias. The regression
suite rerenders these cases, requires exact repeatability within each platform,
and keeps the existing `1e-5` relative RMS tolerance. Muting oscillator 1 must
silence the native fixture; removing external input must silence Audio Input.
S&H Noise and String vary between repeated native and browser captures when the
storage RNG uses its normal clock-derived seed; see the controlled-seed evidence
below. This is default/template fixture
coverage, not complete parameter-mode, unison, modulation, wavetable-library,
live editing or real-time parity. The survey's `complete` field means every
family/rate was measured, not that all oscillator requirements passed.

Validation: both rendering harnesses built; all 20 deterministic oscillator
regressions passed in 56.1 seconds. Six shared-helper regressions also passed,
covering dry FM2, stereo convolution and MSEG HOLD at both rates. The four
S&H Noise/String unseeded survey cases are not counted as passing tests.

`oscillators-seeded.json` records a separate seed-17 survey. Both previously
non-repeatable families draw from `storage.rngGen`: S&H copies its generator,
and String seeds its exciter generator from it. The development harness's
`--storage-seed UINT32` option and `surge_seed_storage_rng` C ABI reset this one
generator immediately before the note, after loading and selecting the oscillator.
Normal application entropy and other independent DSP RNGs are unchanged. These
APIs require exclusive engine ownership and are not audio-thread controls.

All 24 family/rate cases repeat exactly within each platform and match across
platforms at seed 17. Reproduce with
`node web/scripts/oscillator-survey.mjs OUTPUT.json --storage-seed 17`.
The additional regression fixtures exercise S&H Noise and String at seeds 1,
17 and 123456789, with the unchanged tolerance, repeatability checks and a
different-seed control that must materially change the output. These measure
the existing stochastic DSP given identical random sequences; they do not
claim natural-entropy distribution testing or complete oscillator-mode parity.

Both rendering harnesses built. The combined regression run passed 32 checks in
1.5 minutes: the existing 20 unseeded fixtures and 12 controlled-seed S&H
Noise/String cases. Six invalid native seed arguments were also rejected before
rendering. The seed-17 survey's recorded hashes match the binaries used in this
run. Other independent stochastic generators, including pending Airwindows
cases, still require their own validation.

## Effect audio fixtures

`effects.json` distinguishes default/convolution fixtures, explicit parameter
fixtures, and pending families. Parameter fixtures resolve indices from original
C++ enums and require audible differences from dry and default-effect renders.
The native development harness accepts `--audio-input` before effect options to
supply the same deterministic stereo signal as the browser comparison helper.
Native 32-frame rendering is compared with browser buffers of 17, 128, 63 and 32
frames to exercise input continuity across block boundaries.

Vocoder's input fixtures cover all four modulator modes at both sample rates.
The Wasm envelope preserves the ARM64 reference's reciprocal-square-root
approximation. Build and run the native `surge-vocoder-envelope-check` target on
ARM64 to compare its lookup against the actual pinned SIMD implementation. The
check covers every positive subnormal, both ends of every normal-exponent
mantissa bucket, and two million pseudo-random bit patterns. Its `--print-table`
option regenerates the header constants. Native synthesis arithmetic is unchanged.

Default-survey measurements in the registry are historical evidence for those
specific defaults, not measurements of the edited/input fixtures. Passing these
fixtures still does not prove complete effect-parameter or cross-architecture
parity, nor sustained audio-deadline performance.

## Airwindows expansion

`airwindows.json` records all 77 entries from the original streaming registry.
The report separates 61 active fixtures, the two deliberately silent retired
placeholders, and 14 pending cases. Its `coverage.pending` entries distinguish
statistical work and repeatable native/Wasm differences. Nine formerly weak
fixtures now include source-derived parameter edits; eight pass deterministic
comparisons and Deck Wrecka needs statistical validation. The report records
ordered edits and distinguishes `matched-parameter-fixture` from default cases. The
underlying per-rate measurements and binary digests remain available for review.

The regression suite derives selector values and the AD Clip Boost mapping from
source, checks that every entry has both sample rates, and verifies the stored
coverage summary against its measurements. It then rerenders every accepted
fixture against the native engine. Pending cases remain migration requirements;
passing this suite does not turn them into platform limitations or complete
Airwindows UI, parameter or real-time parity.

The active-parameter expansion passed 221 combined parity and serialization
checks in 3.8 minutes. Each edited fixture must change both the dry signal and
the same sub-effect's defaults, and match native rendering at the unchanged
`1e-5` relative RMS tolerance. Both harness builds passed. Twelve entries still
need statistical validation; Dust Bunny and To Tape retain their full-synth
48 kHz mismatches. The earlier full 422-check run predates these 16 added cases.

## Identical-input effect checks

`airwindows-isolated.json` is deliberately separate from `airwindows.json`.
Use `effect-survey.mjs OUTPUT --airwindows --effect-only NAME...` to feed the
same exactly representable input to each platform's effect object. These blocks
bypass voices, routing, modulation and output gain; they do not replace the full
synth comparisons. The report marks `effectOnly: true`, and the ordinary parity
inventory rejects that scope.

AD Clip and Dust Bunny produce identical samples for this fixture at both rates;
To Tape's relative RMS error is approximately `1e-13`. Dust Bunny and To Tape's
full FM2 mismatches remain unresolved. A trial Dust Bunny interpolation FMA and
a trial To Tape long-double precision conversion did not reduce those errors;
neither DSP change was retained. Native captures are byte-identical after adding
the isolated comparison API. Further investigation must distinguish upstream
sample differences from effect behavior rather than relaxing a shared tolerance.

Validation: the combined full-synth, isolated-effect and serialization run passed
205 checks in 3.6 minutes. Both browser and native reference builds passed.
The subsequent full browser workflow run passed 422 checks in 11.9 minutes on
Chrome 153.0.8010.53 with `SURGE_TEST_SILENT_OUTPUT=1`. Neither result closes the
pending audio cases, hardware verification or remaining portable-feature inventory.

## Wavetable context controls and export handoff

`wavetable-context.spec.js` exercises the original oscillator menu for display-name
renaming/cancellation, Unicode patch save and reload, refreshing user tables,
opening the script editor, and all five export routes. Export checks validate
WT/WAV samples, frame counts, metadata, Serum/VCV resolutions and preservation of
the selected table. Script-only export choices remain hidden for non-scripted
tables. These nine reviewed entry points bring the ledger to 113 reviewed and
623 unreviewed; this is not completion of every wavetable workflow.

The checks exposed a browser handoff bug when a native export callback normalized
a filename extension. The bridge now finds a single generated file inside that
operation's fresh private directory if the original path was renamed. It never
guesses between multiple outputs. Missing/ambiguous output is reported before
opening the destination writer, and a failed write retains the normalized file
through browser-storage reload. The selected browser handle still controls the
external filename.

The rebuilt application and JUCE harness passed 25 targeted export/context checks
in 1.4 minutes, followed by the non-scripted-menu visibility check. Earlier four
context checks also passed before the handoff change. Physical file-picker and
filesystem-device checks remain separate from these automated picker fixtures.

Non-scripted export now uses the same authoritative snapshot helper as undo and
clipboard capture. An undo/clipboard replacement can be current on the control
thread while the audio thread still renders the previous buffers; copying those
old buffers directly could export stale samples. The snapshot is independent of
later edits and survives the asynchronous save picker. Three browser cases pass
for inactive, running and suspended audio: undo restores a resliced factory
table, export opens a picker, redo replaces the table, and the saved WT retains
the restored dimensions and every sample.

Those UI cases alone do not force the pending-publication interval. A separate
development-only Wasm check creates a replacement with no audio consumer and
asserts that the live data stays old while the captured/exported/reimported data
is new. It also checks superseded tokens, uncommitted worker results, subsequent
adoption and independence from a newer replacement:

```sh
cmake -S . -B build-web
cmake --build build-web --target surge-wavetable-snapshot-check -j4
node build-web/checks/surge-wavetable-snapshot-check.cjs
```

The check passes against the real browser storage implementation. Its Node target
is a test harness under `build-web/checks`, outside the static application package;
it adds no runtime service or companion application. Configure the browser build
with the pinned toolchain first, as described in the main build instructions.
