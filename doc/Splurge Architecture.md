# Splurge: Architecture & Design Document

**Status:** Revised after codebase verification audit — captures all design decisions through the verification pass (Surge sst-* library audit, oscillator/effect dependency audit, importer ground-truth audit).
**Scope:** Architecture and design only. Work breakdown (epics, tasks, sequencing) is intentionally **out of scope** for this document and will be produced as a separate planning effort using this document as input.
**Constraint:** This document contains no code. It references Surge XT source locations in Appendix A as orientation for planners.

---

## 1. Vision & Goals

Splurge is a new software synthesizer derived from Surge XT. It is not an incremental fork: it lifts Surge XT's proven DSP modules into a new, semi-modular architecture with a rebuilt engine, patch format, and user interface.

### 1.1 What we preserve from Surge XT

These qualities motivated starting from Surge rather than from scratch:

- Free and open-source.
- Integration of high-quality third-party DSP, notably the Airwindows effect suite (which continues to evolve upstream).
- A large existing preset library (made available to Splurge via one-way import).
- Deep, interesting individual modules: a rich oscillator roster, an unusually large waveshaper library, MSEG and Lua-scriptable modulators, file-based scale/tuning support via the `tuning-library` (proprietary MTS-ESP is not adopted — §4.5).
- Cross-platform operation (Windows, Linux, macOS) as VST3 and CLAP (and AU via the framework).

### 1.2 What changes

The four pillars of Splurge, each elaborated in later sections:

1. **Total visual feedback** (§14): every visible control always reflects its effective, modulated state — composite modulation ranges color-coded by modulator type, current-value cursors, modulation-aware displays — at no cost to GUI frame rate. Inspiration: Serum, Phase Plant, and their ancestor Massive — but taken further than any of them.
2. **Semi-modular lane architecture** (§5–§7): the fixed pipeline (fixed counts of oscillators, filters, effects, modulators, and the two-scene structure) is replaced by a single universal container — the **lane** — supporting any number of oscillators, effects, and modulators per patch, with serial/parallel routing, while defaulting to a sane subtractive signal flow that requires zero routing effort from the casual user. We deliberately push as close to fully modular as possible **without** becoming a pure graph editor — this is not Reaktor, and that boundary is intentional.
3. **Oscillators as modulation sources** (§9): oscillator audio output is routable to parameters, making FM and phase modulation general routing facts rather than fixed topology presets.
4. **Reorganized content** (§10.6, §11.4): effects unified into one credit-annotated taxonomy (no separate "multieffects" silo); presets unified under one categorized factory tree with inline author credit and first-class tags.
5. **Programmatically authorable patches** (§11.1): a structured, machine-readable patch format (JSON) so tools, scripts, and language models can compose patches directly — not just the GUI.

### 1.3 Non-goals

- **No Splurge → Surge backward compatibility.** Import is one-way (Surge XT presets into Splurge). Splurge presets are not loadable in Surge.
- **No pure graph editor.** Semi-modular, with sane defaults, always.
- **No mobile or WebAssembly version in the initial release.** Viability was assessed and is recorded in §4.3; the long-term vision (a full-fat synth playable away from the studio, with presets that copy back to the desktop plugin) remains on the wishlist.

---

## 2. Guiding Principles

These principles were established during design and should govern all downstream decisions:

1. **Splurge's data model serves Splurge.** Surge import is a one-way translator that converts Surge's specific structures into Splurge's general primitives. The translation may be lossy in *form* as long as it is faithful in *audio result*. "Surge users expect X" or "Surge does it this way" is never a valid justification for a Splurge design choice; "Splurge is a better synth if X" is the only valid form. The importer is allowed to be complex; the Splurge data model is not.
2. **Globals become per-unit by default.** Anywhere Surge has a single patch-wide or scene-wide setting that conceptually belongs to a module (oscillator, filter, effect, modulator), Splurge makes it a parameter on that module. A short list of genuinely patch-wide settings (e.g., master polyphony cap, master tuning) is acceptable; the default disposition is per-unit. Where a patch-wide *default* is convenient, it is implemented as a propagated default with per-unit override, not as a singular value (§9.11). The "Character" parameter, voice-mode flags, and similar Surge globals are explicitly per-unit in Splurge.
3. **Sane defaults over modular ceremony.** A user following the path of least resistance gets a conventional subtractive synth without touching routing. Modular power is opt-in.
4. **Primitives and packaged effects are both first-class.** Whenever a packaged effect (e.g., a distortion unit) is built around a reusable kernel (e.g., a waveshaping function), both the primitive and the package ship as separate modules (§10.5).
5. **Visual truth.** Anything that has a visible control reflects its current effective state in that control's visible state — always, not only when a modulator is selected.
6. **Performance optimizations must not shape user-facing architecture.** SIMD voice batching and threading are engine concerns hidden behind module context metadata (§6, §8); the user never arranges modules *for* the optimizer, though the UI may gently surface why a module fits certain contexts.
7. **Framework independence of the core.** The module library and engine core are plain C++ with no GUI-framework types in their public interfaces. The framework (JUCE) is confined to the GUI layer and host adapters (§4).
8. **Import compatibility by construction.** Module identity, parameter schemas, and behavior evolve under strict additive rules (§10.3) so that every Surge XT preset remains importable forever, and Splurge presets remain loadable across Splurge versions. Preset compatibility is a **one-way hard guarantee**: every preset ever saved loads in every future Splurge; loading newer presets in older versions is explicitly not promised (§11.6).
9. **Lift discipline.** Lifted code carries forward only what serves Splurge's design. Surge features built outside the spirit of a clean modular architecture (e.g., the Alias oscillator's modes that synthesize from arbitrary patch memory) are dropped during the lift, with import warnings for affected presets (§3.5). Favor simplicity over forced fidelity to Surge quirks.
10. **Accessibility from the start.** Surge XT has a genuine accessibility story; Splurge designs accessibility in from day one rather than retrofitting (§14.8).
11. **Programmatic authorability.** The patch format is a structured, machine-readable document (§11.1) — composable by tools, scripts, and language models, not just by the GUI.

---

## 3. Strategic Approach: Consume, Lift, and Build

### 3.1 Three tiers of code reuse

A code review of Surge XT (~163k lines across ~362 source files) plus an audit of its submodule libraries revealed that the DSP code Splurge needs splits cleanly into three tiers, not two. This restructures the "lift" strategy materially:

**Tier A — Consume as standalone libraries (no lift needed).** The Surge team has already extracted much of the DSP into independent, GPL-3 `sst-*` submodule libraries that compile and ship without Surge:

- `sst-filters` — the entire per-voice quad-SIMD filter library, halfband resamplers, biquads.
- `sst-waveshapers` — the 40+-entry transfer-function library.
- `sst-effects` — 11 effects (Reverb 1 & 2, Delay, Flanger, Phaser, Floaty Delay, Bonsai, Rotary Speaker, Treemonster, Nimbus, Spring Reverb) already factored behind a template-config adapter pattern.
- `sst-basic-blocks` — DSP primitives, the sinc-table provider, RNG, clippers, resamplers, parameter-metadata helpers.
- Mutable Instruments `eurorack` (MIT) — Clouds/Plaits, used by Twist oscillator and Nimbus effect.
- Airwindows (MIT) — vendored under `libs/airwindows`.
- `tuning-library` (GPL-3) — Surge's scale/tuning support (Scala-format files, KBM keyboards).
- `LuaJIT` (MIT) — script host for the Formula modulator and wavetable scripting.
- (Not adopted: ODDSound MTS-ESP — see §4.5 for the open-source-microtonality decision.)

**Tier B — Lift directly (un-extracted Surge code).** The DSP that has *not* been factored upstream remains in `src/common/`. Splurge lifts these into a vendored static library:

- **Oscillators (12 types)** — Classic, Sine, Wavetable, S&H Noise, Audio Input, FM3, FM2, Window, Modern, String, Twist, Alias. Each its own .h/.cpp pair under `src/common/dsp/oscillators/`.
- **Modulation system** — `ModulationSource`, `LFOModulationSource`, `ADSRModulationSource`, the MSEG evaluator (`MSEGModulationHelper`), the Formula evaluator (`FormulaModulationHelper`), step sequencer, controller/macro sources. None of this is in `sst-basic-blocks`; it's Surge-internal code in `src/common/dsp/modulators/`.
- **Lipol smoothing** — Vember Tech's SIMD parameter-smoothing library at `src/common/dsp/vembertech/lipol.h` (~1000 lines, not in `sst-basic-blocks`).
- **Wavetable data structures and I/O** — `src/common/Wavetable.{h,cpp}`.
- **Parameter class** — `src/common/Parameter.{h,cpp}` (the parameter-bag interface; §3.2). Self-contained enough to lift (explicitly designed to operate with a null storage pointer for non-UI uses).
- **Non-adapted effects** — Distortion, Vocoder, Convolution, Mid-Side Tool, EQ 3-Band, Graphic EQ 11-Band, Ring Modulator, Frequency Shifter, Resonator, Conditioner, Chorus, Tape, Combulator, CHOW, Neuron, Exciter, BBD Ensemble, Audio Input, and the Airwindows wrapper — those not yet ported to `sst-effects`.

**Tier C — Build fresh.** The synthesizer above the module boundary is entirely new code:

- Engine core (lane runtime, voice runtime, modulation routing, parameter/effective-value system, signal-bus types, SIMD batching layer, optional thread scheduler).
- Engine-context shim (§3.3) providing the service surface the lifted modules require.
- Patch format and persistence (§11).
- Surge XT importer (§12).
- GUI (editor, widget library, lane editor, patch browser).
- Host adapter shells (thin, via JUCE).

**Conclusion adopted:** Surge's intellectual value lives in the modules. The orchestrator, voice class, GUI, and patch model are precisely the parts this project is dissatisfied with. Therefore: **consume Tier A, vendor Tier B as a static library, build Tier C fresh.** Doing the rewrite in place — incrementally refactoring the orchestrator while keeping compatibility with a schema we don't want — was evaluated and rejected as slower and dirtier than a fresh build consuming the lifted modules.

### 3.2 The module boundary: parameter bags plus scene-array indexing

The lifted modules declare their parameters into per-module storage structs (`OscillatorStorage`, `FxStorage`) using Surge's `Parameter` class. **Decision:** keep those parameter-bag structs and the `Parameter` class as part of the module library's public surface, rather than redesigning the modules' parameter declaration mechanism. Rationale:

- Dramatically less work than decoupling parameter declaration from storage.
- Surge import becomes nearly mechanical: every parameter value in a Surge patch populates the identical struct the Splurge module reads.
- The new engine is free to own, allocate, and arrange these parameter bags however it likes (dynamically, inside lanes) without touching module internals.

**Caveat established by codebase audit:** oscillators do **not** read their parameter bags directly at audio rate. They cache `param_id_in_scene` indices at init time and then read parameter values from a **scene-wide `pdata*` value array** (`localcopy`) using those indices. The engine has two options for handling this without modifying the modules:

- **Emulate the scene-array layout per-lane:** every oscillator-bearing lane carries a `pdata*` array large enough to hold the union of its modules' parameter indices, populated each block as Surge populates `localcopy`. The lifted oscillators continue to read from this array unchanged.
- **Mechanically rebase the indices to per-module:** a small patch to oscillator init turns the indices into per-module-local offsets, and the lane carries a small array per module. Smaller per-block memory footprint, but touches each oscillator.

Engine epic-planning should pick one and document; the first is simpler and more conservative.

**Lift hygiene:** modules must compile without the Surge orchestrator, voice class, or GUI. Globals, static-init-guarded lookup tables (notably the Alias oscillator's `shaped_sinetable`), and reaches into the patch hierarchy (§3.3) are wrapped, moved into the engine context, or — when the reach is outside the spirit of the design — dropped (Principle 9). A standalone test harness that instantiates a single module, feeds it input, and verifies output is the regression net for the entire project.

### 3.3 The engine-context surface

The lifted modules are not pure functions; they read shared services from what is currently `SurgeStorage`. Splurge's engine provides a replacement **engine context** — a single small interface the modules consume in place of `SurgeStorage*`. The audit identified the following services as the minimum surface area:

- **Pitch/tuning tables:** `note_to_pitch()`-style lookups; scale/tuning resolution via the `tuning-library` (Scala/KBM file loading). No MTS-ESP client; see §4.5 for the broader microtonality protocol question.
- **Sinc tables** (`sinctable`, `sinctableI16`) — audio-rate-critical convolution kernels.
- **Sample-rate state:** `samplerate`, `samplerate_inv`, `dsamplerate_os_inv` (instance members in Surge — no global samplerate variable exists, which is a relief).
- **Tempo:** `temposyncratio`, `temposyncratio_inv`.
- **RNG:** `rand_01()`, `rand_u32()` — referenced by multiple oscillators for drift, noise, randomization.
- **Memory pools:** the string-delay-line pool consumed by String oscillator (`memoryPools->stringDelayLines`).
- **Audio input buffers:** consumed by Audio Input oscillator and Alias's audio-buffer mode (`audio_in[][]`).
- **The `Character` parameter** — Surge's global tone-shape filter. In Splurge, this becomes a **per-oscillator parameter** (Principle 2; §10.7) and the importer fans out the global value to all imported oscillators. No global Character in the engine context.

The `sst-effects` library defines a similar pattern (a `Config` adapter providing `GlobalStorage`, `EffectStorage`, `ValueStorage` template arguments — exemplified by the existing `SurgeFXConfig` in Surge XT). Splurge's engine context aligns with this convention so `sst-effects` integration uses the same shim.

**Sub-decisions captured here for planners:**

- The Alias oscillator has modes (`aow_audiobuffer`, `aow_scenedata`, `aow_dawextrastate`, `aow_stepsequences`) that synthesize from arbitrary patch memory regions. Per Principle 9, these modes are **dropped** in Splurge; patches relying on them generate an import warning and fall back to a benign Alias mode. (Decision: §17 open question on whether to preserve audio-input mode specifically.)
- The Alias oscillator's `shaped_sinetable` (static lookup, non-thread-safe init guard) is moved into engine-controlled init during context construction.

### 3.4 What is built fresh

- **Engine core** — lane runtime, voice runtime, modulation routing, effective-value caching, typed signal buses, SIMD batching layer, optional thread scheduler.
- **Engine context** — the shim defined in §3.3.
- **Patch format and persistence** (§11).
- **Surge XT importer** (§12).
- **GUI** — editor, widget library, lane editor, patch browser (§14).
- **Host adapter shells** — thin, via JUCE.

### 3.5 Repository organization

A structure along these lines (names indicative):

- `modules/` — Tier B vendored DSP from Surge: oscillators, modulation classes, MSEG/Formula evaluators, lipol, wavetable I/O, the non-adapted effects, the Airwindows wrapper. Evolves under the rules of §10.3. Plus build-time dependencies on Tier A (`sst-*`, `eurorack`, etc.) consumed as ordinary libraries.
- `engine/` — Tier C: lane graph runtime, voice runtime, modulation routing, parameter/effective-value system, patch model, engine context, persistence.
- `ui/` — editor, widgets, browser (JUCE permitted here).
- `adapters/` — plugin shells: VST3, CLAP, AU, standalone (JUCE permitted here).
- `importers/surge-xt/` — the one-way Surge preset translator; the only place Surge-specific knowledge lives outside `modules/`.

### 3.6 Known costs of the rewrite path (risk acknowledgment)

Rewriting the orchestrator forfeits years of accumulated fixes in areas that are easy to get 80% right and hard to finish:

- Host integration edge cases: automation, latency reporting, sample-rate changes, freeze/render, MPE corner cases.
- Voice stealing, glide/portamento, monophonic legato modes.
- Undo/redo and automation recording.
- Accessibility (mitigated by designing it in from the start).
- The long tail of importing every factory preset correctly.
- Reproducing module behavior under the new engine context — especially anywhere the lifted module silently depended on a `SurgeStorage` service we paraphrased.

These are accepted costs, tracked in the risk register (§16).

### 3.7 Upstream contribution stance

Where Splurge work product happens to be upstreamable to Surge XT (bug fixes in lifted modules, new module algorithms that don't depend on Splurge's engine, additional effects suitable for `sst-effects`), we keep it separable — by directory convention and/or build-time definition — so contributions back to Surge remain possible. With the fresh-build approach this is an aspiration rather than a structural driver; the mechanics are an open item (§17).

---

## 4. Technology & Framework Decisions

### 4.1 JUCE retained

Alternatives were evaluated (iPlug2; DPF; raw CLAP/VST3 SDK plus a UI library such as Slint, VIZIA, or Qt; the Rust ecosystem). **Decision: stay with JUCE.** Rationale: it is the path of least resistance to a *finished* product for a developer who doesn't already know an alternative framework — the most mature widget library, accessibility support, MIDI/MPE/automation glue, and plugin-format adapters in the audio space. The licensing concern (GPL/commercial dual license) and the immature mobile/WASM story are accepted trade-offs, revisitable later precisely because of §4.2.

### 4.2 The framework-independence rule (hard rule)

- JUCE types may appear **only** in `ui/` and `adapters/`.
- The module library and engine core expose plain-C++ interfaces with no framework types. Any JUCE leakage found in lifted modules is wrapped or removed during the lift.
- Consequence: a future framework change, a headless build, or a mobile build touches only the shell and UI layers, never the synth itself.

### 4.3 Mobile / WebAssembly viability (recorded, deferred)

- The DSP is portable C++ with SSE intrinsics; porting means an SSE-to-NEON shim or scalar fallback for the SIMD layer — bounded work.
- The UI is the real cost: a touch UI is effectively a full additional UI project, and JUCE's Android and WASM stories are rough.
- Decision: out of scope for the initial release; not a design constraint; the framework-independent core preserves the option.

### 4.4 Plugin targets

VST3 and CLAP as primary targets; AU and standalone via JUCE as the framework makes them cheap. Block-based processing with oscillator-stage oversampling is inherited from the lifted DSP conventions.

### 4.5 Licensing

- **Splurge is GPL-3.0-or-later.** Surge XT itself is GPL-3.0-or-later, and every Tier A `sst-*` library is GPL-3 as well; a fork cannot license more permissively. The framework-independence rule (§4.2) preserves the option to relicense the engine and module library in a future re-implementation, but the initial Splurge codebase as shipped is GPL-3.
- **JUCE** is dual-licensed (GPL-3 + commercial). Under GPL-3 it is freely usable for Splurge.
- **Airwindows** (MIT), **Mutable Instruments `eurorack`** (MIT — used by Twist and Nimbus via Surge's MI fork), **LuaJIT** (MIT), **`simde`** (MIT — already vendored, providing SSE→NEON portability headers we benefit from on ARM), **`fmt`** (MIT), **PEGTL**, **`zstd`** (BSD), **SQLite** (public domain), **CLI11** (BSD-3) are all GPL-3-compatible.
- **`tuning-library`** is GPL-3.
- **MTS-ESP (ODDSound) — dropped.** MTS-ESP is not a standard open-source library; it is a client for ODDSound's commercial microtonality service under restrictive terms. **Decision:** Splurge does not adopt MTS-ESP. Native scale/tuning support carries over via the GPL-3 `tuning-library`. Real-time client-side microtonality protocols (the role MTS-ESP fills in Surge) are an evaluated open question for v1.x (§17): candidates include Sevish/Scala-format file loading at patch level (already available via `tuning-library`), an open-source MTS-ESP-compatible client implementation if one emerges, or designing Splurge's own simple host-protocol path. Until that decision lands, Splurge ships with file-based scale/tuning support only.

---

## 5. The Lane Model (Core Architecture)

### 5.1 The lane is the only structural container

A **lane** is the sole first-class container in a patch. A lane holds:

- **Oscillators** — zero or more.
- **Effects/processors** — zero or more, in a defined serial order.
- **Modulators** — zero or more, each scoped per-voice or per-lane (§9.2).
- **Voice configuration** — polyphony mode (poly/mono/legato/latch), voice count, key range, MIDI channel filter, glide/portamento settings.
- **An output target** — another lane, or master.
- **A user-assignable name**, and a read-only **input label** naming the lane's source (or the generic `<multi>` when more than one source feeds it).

The voice configuration is **dormant** when the lane contains no oscillators: such a lane allocates no voices and acts as a pure processing bus, and the voice-config UI simply does not apply. A lane with oscillators allocates voice instances and manages its own polyphony.

A lane's audio originates from one or more of **three source kinds**:

- **Generators** — oscillators, samplers, noise. Note-driven: the lane's voice allocator creates voice instances at note-on, and generators instance into them.
- **External inputs** — audio from outside the synth (effects-mode hosting, sidechain inputs). Continuous signals with no note lifecycle; voice count of one.
- **Lane inputs** — audio routed from upstream lanes, arriving at whatever granularity the connecting bus delivers (§5.4).

Channel layout (mono/stereo/multichannel) is a **property of the source** — the audio-input module declares its layout; generators render into the lane's layout. Voice multiplicity is a property of note allocation and unison (§7.5). These are independent axes (§5.4).

There is deliberately **no** "scene," "voice group," "instrument layer," or other second container concept. An earlier design iteration proposed a "voice group" abstraction; it was recognized as collapsing entirely into lanes (separate sets of oscillators in separate lanes, recombining at master) and was removed. Surge's Scene A/B mechanic is likewise not a Splurge concept — it is reconstituted *only* by the importer as two ordinary lanes (§12.3).

### 5.2 The patch

A patch is:

- An ordered list of **lanes**.
- **Macros** (patch-level, so they can modulate parameters in any lane; modulatable themselves — §9.2).
- **Global modulators** (mod wheel, aftertouch, MIDI CCs, pitch bend, keytrack-style sources — addressable from any lane).
- **Metadata**: name, author, license, comment, category, tags (§11.3).
- **Embedded or referenced resources**: samples, wavetables, impulses (§11.2).

### 5.3 Routing and topology rules

- **Cycle prevention by construction:** a lane may route only to a **higher-numbered lane** or to **master**. This numbered-lane rule was chosen over free-form routing with runtime cycle detection for its simplicity and predictability; it makes circular routes impossible rather than detected.
- **Serial vs. parallel is an emergent property of routing**, not a mode switch: lanes that feed other lanes form serial chains; lanes that feed master directly run in parallel; combinations of both are free.
- **Merging** is implicit: multiple lanes targeting the same destination sum there, and the destination's input label reads `<multi>`.
- **Sends collapse into routing:** there is no separate send/insert mechanism. A "send" is simply a lane that receives routed input (optionally at a routing weight) from one or more other lanes. Surge's send slots import as exactly this (§12.3).

### 5.4 Typed lane signal buses: the two-axis model

The connection between lanes is a **typed signal bus**, not a hardcoded stereo pair. A bus — and a lane's internal signal — is structured as a small matrix along two **orthogonal axes** with different semantics and different collapse rules. (An earlier design iteration considered unifying voices and channels into a single strand count — "a stereo input is two voices" — and rejected it: the two axes collapse under different rules, and merging them makes boundary behavior ambiguous.)

**The voice axis** — source-side multiplicity: "how many simultaneous instances of this sound exist?"

- Voices are *ephemeral*: created by note-on, multiplied by unison (§7.5), destroyed at release. External inputs have a voice count of one (no note lifecycle).
- Voices carry **lineage** — each voice knows its note instance and its unison-group ancestry (note → unison group → clone). Lineage is what makes collapse levels well-defined.
- The voice axis collapses by **summation**, which is always semantically safe (it is simply mixing). Collapse happens at **mix points** (§7.4) to one of three levels: **single stream** (sum everything), **per-note** (sum each note's clones; keep notes separate), or **per-voice** (keep every clone separate).

**The channel axis** — destination-side geometry: "where does this signal sit in space?"

- Channels are *static*: L is always L; their natural endpoint is the output layout. Layouts: **mono**, **stereo** (default), **multichannel** (surround, ambisonic, or others — adding a layout is a matter of defining the type and its adaptation rules, not re-architecting).
- Channel layout is a **source-side property** (§5.1). Within a lane's per-voice phase the layout is fixed per lane (default stereo); each voice renders into it (per-voice stereo with per-voice pan, as in Surge).
- The channel axis adapts by **layout rules** at lane boundaries when producer and consumer layouts differ: mono→stereo duplicates or center-pans; stereo→mono sums with a pan law; multichannel uses defined matrices. These are the standard adaptations every DAW applies at every pin connection — well-trodden, nothing novel.

The axes never interact: unison multiplies voices and never touches channels; pan and width operate on channels within each voice; mix points sum voices; layout adaptation happens only at boundaries. Worked examples:

| Signal | Voice axis | Channel axis |
|---|---|---|
| Stereo external input | 1 | 2 |
| Mono-rendering oscillator with unison 3 | 3 | 1 |
| Stereo-rendering oscillator with unison 3 | 3 | 2 |
| …followed by a Unison module ×3 (§7.5) | 9 | 2 |
| A held 4-note chord on the above | 36 | 2 |

Boundary resolution is deterministic: voices sum to the granularity the consumer requested; the channel layout adapts by rule. A 3-voice × 2-channel lane feeding a plain stereo bus performs one summation along the voice axis and no channel work at all.

The initial release may implement only stereo with single-stream and per-note collapse; per-voice granularity and multichannel layouts are designed in from the start so they are additive features, not refactors.

### 5.5 Multiband splitting is a lane primitive

The requested "effect that splits a signal into bands routed to different destinations" is **not an effect** in Splurge; it is a routing primitive: a splitter lane (typically oscillator-free) takes one input and produces multiple band outputs, each routed to a different downstream lane. The bands recombine wherever those lanes converge. This falls out of the lane model rather than requiring a special effect API with multiple outputs.

### 5.6 The default experience

A new patch contains one lane with a conventional subtractive layout (oscillator → filter → output to master) and sensible default modulators. The casual user never sees routing; the lane mechanic earns its keep only when the user wants layers, splits, parallel processing, multiband chains, or unusual filter topologies.

### 5.7 Example topologies

| Scenario | Lane configuration |
|---|---|
| Basic patch | One lane: oscillators + voice config + FX → master |
| Five-layer stack | Five lanes, each with own oscillators and voice config → master |
| Imported Surge patch | Lanes constructed from each populated Surge scene; key/channel filters configured per Surge's split mode (§12.3) |
| FX bus / send | An oscillator-free lane receiving weighted input from other lanes |
| Multiband processing | Splitter lane → three band lanes (each with its own FX) → master |
| Mono lead over poly pad | Two lanes with different polyphony modes |
| Per-note FX | Consuming lane's input mix point set to per-note collapse; each chord note's signal is processed independently (§7.4) |
| Per-unison-voice processing | Mix point set to per-voice collapse; each unison clone of each note is processed independently (§7.4) |
| Extreme unison stacking | Unison modules in series multiply voice counts (3 × 3 = 9 clones per note) (§7.5) |
| Dual-mono / mid-side chains | Channel-splitter or M/S utility routes each channel to its own lane; recombine downstream (§10.5) |
| Feedback resonance | An audio-rate routing edge from a downstream module's output back into a feedback-input port on an earlier module in the same lane (§6.6) |

---

## 6. Lane Processing Phases

### 6.1 The real distinction: per-voice vs. post-mix

Investigation of Surge's quad-SIMD filter architecture established that the meaningful architectural boundary is **not** "filter block vs. FX section" — it is **per-voice processing context vs. post-mix processing context**. A lane containing oscillators implicitly has two phases:

- **Phase 1 — per-voice:** modules run inside the voice loop, once per sounding voice. Voice-batchable modules are SIMD-batched across voices (§8.1). Per-voice modulators (envelopes, voice-scoped LFOs) apply here. Eligible modules: oscillators, voice-batchable filters, voice-batchable waveshapers/saturators, and any future module written for this context.
- **Phase 2 — post-mix:** the lane's voices have been summed into a single stream. Modules run once per block on that stream. Anything can live here: Airwindows algorithms, reverbs, complex effects. Only lane-scope and patch-scope modulators apply.

A lane without oscillators is pure phase 2 from its first module.

### 6.2 The transition is automatic and visible

The phase 1 → phase 2 transition (the "voice mix") occurs **automatically at the first module in the chain that requires post-mix context**. The user does not manage it, but the UI renders it as a visible divider in the lane so the structure is legible. If the user inserts a post-mix-only module early in a chain, the divider simply moves; nothing breaks. Power users may force an earlier transition (e.g., by dragging the divider or inserting an explicit voice-mix point) — a rare need.

The divider is a **mix point** in the sense of §7.4: it carries the collapse-level setting (single stream / per-note / per-voice) governing what survives into phase 2. The default is a full sum to a single stream — the conventional behavior.

### 6.3 Module context capability

Every module type declares which contexts it supports: **per-voice**, **post-mix**, or **both** (with a separate implementation per context where applicable; the engine selects by placement). This resolves a Surge oddity the design review surfaced: Surge's per-voice filter menu differs from its FX-section filter menu (e.g., no Airwindows filters per-voice) not because those filters are fundamentally different, but because each was *implemented* for one context — per-voice filters are written branchless and vectorizable for voice batching; FX filters are written for a single stereo stream. In Splurge:

- Menus are **unified** (one filter list, one waveshaper list, etc.).
- Each entry carries an indicator of where it can live (per-voice / post-mix / both).
- Friendly, intuitive terminology for *why* a module fits certain contexts is a UI-design task, deferred (§17).

> **Validation note (Appendix B):** context capability is also the extension axis for the module library — B.9 identifies a **per-voice-context delay** (for do-it-yourself Karplus-Strong combs) as a gap the architecture already accommodates: it is just a delay module declaring per-voice support, no engine change required.

### 6.4 Cross-phase modulation constraint

Per-voice modulators cannot modulate phase 2 parameters *per voice* — after the voice mix there is no voice scope. They may contribute only as an aggregated (summed) control signal. The UI must surface this clearly when a routing edge crosses the phase boundary.

### 6.5 Filters are individual modules

An earlier design iteration proposed lifting Surge's two-filter-plus-routing structure as a single "filter block" module. This was **rejected** (it was justified by Surge's mental model, violating Principle 1). Final decision: **filters are individual, first-class modules** in the lane chain. There is no "filter block," no filter-topology enum, no privileged filter positions, and no Surge filter-mode menu. The SIMD optimization that motivated the filter block moves into the engine as generalized voice batching (§8.1).

Topology decisions that Surge expressed as filter-block routing modes (serial 1/2/3, dual 1/2, ring, stereo, wide) are not Splurge concepts. They decompose into ordinary Splurge mechanisms:

- **Serial variations** — module ordering within a lane, plus the feedback mechanism in §6.6.
- **Parallel / dual** — sub-lane splits that recombine.
- **Ring combinations** — a ring-mod module (§10.5) used as a combine point.
- **Stereo** — the typed-bus channel layout (§5.4).
- **Wide** — a stereo-width module (§10.5) used as a post-process.

The importer (§12.4) translates Surge's eight filter configs into these forms.

### 6.6 Feedback as a routable input

Surge's filter-block feedback (the soft-clipped path that loops the filter chain's output back to its input on most filter configs) is a fundamental subtractive-synth behavior that Splurge needs as a general primitive, not a filter-block special case. Implementation:

- Modules whose DSP supports feedback (filters; delay-based effects; certain shapers) **declare a feedback-input routing destination** in addition to their normal parameters.
- Any audio-rate signal can be routed into this destination — typically the output of a later module in the same lane, creating a feedback loop.
- The loop's delay is **one block** (the engine cannot feed a signal forward into a module that hasn't been computed yet within a single block). This matches Surge's existing feedback behavior, which is also block-delayed.
- A soft-clip primitive on the feedback path is the module's responsibility (filters and delays inherit Surge's soft-clipper from the Tier B lift; new modules implementing feedback inputs should follow the same convention).
- The "no cycles" rule on lane-to-lane routing (§5.3) is preserved; intra-lane feedback is a per-module facility that does not violate it, because the 1-block delay makes the edge well-defined.

This makes Splurge's resonance and feedback architecture more general than Surge's: any feedback-capable module can be wired into any feedback path, not just the fixed scene-feedback loop.

> **Validation notes (Appendix B):** the one-block-delay rule is load-bearing in three catalogue entries — B.7 (per-sample operator feedback must stay oscillator-internal; routed self-feedback is audibly different), B.9 (Karplus-Strong from primitives works *because* the feedback edge plus keytracked delay time compose), and B.11 (mutual per-sample cross-modulation networks are excluded — one direction of any loop is always a backward edge, and that is the semi-modular boundary working as intended).

---

## 7. Voices & Polyphony

### 7.1 Per-lane voice configuration

Each oscillator-bearing lane owns: polyphony mode (poly, mono, legato variants, latch), voice count limit, key range, MIDI channel filter, glide/portamento. Layers and keyboard splits are just lanes with overlapping or disjoint ranges/channels — there is no separate split/layer feature.

### 7.2 Voice runtime (built fresh)

The engine's voice runtime handles: note on/off and voice allocation per lane; voice stealing; retrigger behavior; glide/portamento; MPE (per-note pitch bend, timbre, pressure); MTS-ESP and native tuning (lifted from Surge's tuning integration). This is the highest-concentration area of audio-domain edge cases being rewritten rather than inherited; it is flagged accordingly in the risk register (§16).

### 7.3 Per-voice state sizing

Surge allocates oscillator state into fixed-size placement buffers. Splurge must **not** carry a fixed per-voice state cap: future modules (notably the granular oscillator, with many grains in flight) have much larger and variable per-voice footprints. Voice state allocation must be dynamic or generously tunable from the first engine design.

### 7.4 Voice collapse at mix points

The voice axis (§5.4) is collapsed at **mix points** — not configured on sources, and not configured on lanes as a whole. (Both alternatives were considered and rejected during review; see below.) A mix point is any place where summation can occur:

- the **phase divider** inside a voice-bearing lane (§6.2), and
- the **input of a consuming lane** — each lane-to-lane connection carries its own setting.

Each mix point selects a collapse level, defined by voice lineage (note → unison group → clone):

- **Single stream (default).** Sum everything; downstream sees one signal. Matches a conventional FX bus.
- **Per-note.** Sum each note's unison clones; keep notes separate. A held chord is processed as independent per-note signals — modulation can vary per-note, a filter rings differently per-note. (Phase Plant's "Poly" button, generalized.)
- **Per-voice.** Keep every clone separate; each unison voice of each note is processed independently. The highest-fidelity and most expensive level.

Design conclusions, settled during review:

- **The setting does not belong to the source.** Two reasons: multiple generators share the lane's voices (a note-voice instances all of them together, so per-generator granularity settings would conflict), and multiple consumers may consume the same lane at *different* granularities — a reverb send takes the summed stream while a per-note filter lane takes the per-note bundle, simultaneously. Sources determine what *exists* to collapse (voice config creates notes; unison creates clones — §7.5); mix points determine what *survives*. Channel layout, by contrast, genuinely is source-side (§5.1).
- **Module instancing follows the surviving strands.** In a region operating per-note or per-voice, each module runs one instance per strand. Modules with per-voice context support (§6.3) additionally get SIMD voice batching and voice-scoped modulation; other modules run as ordinary independent instances per strand.
- **Validity.** Per-note or per-voice collapse on a connection requires the producing lane to still carry voice lineage at its output (its own divider hasn't already collapsed below the requested level) and the lanes to share voice scope. The engine validates at routing time; connections that can't satisfy the request run at single-stream.
- **Cost.** Carrying N strands between lanes multiplies memory and CPU. The user opts in deliberately per mix point, and the UI surfaces the cost.

Single-stream is committed for v1. Per-note is the highest-priority extension; per-voice follows. The typed-bus abstraction enabling all three is baked into v1 (§13).

### 7.5 Unison: an engine facility, not an oscillator secret

Surge implements unison inside its oscillators (per-sub-voice detune/pan/drift arrays summed internally into one output buffer) — efficient, but it makes unison a privilege of oscillators that happen to implement it. Design review concluded that unison is ~90% **voice cloning with seeded variations** (pitch detune, stereo spread, phase offset/randomization, center/side level blend) and only ~10% oscillator-internal niceties (per-sub-voice drift, the sine oscillator's per-sub-voice feedback). Splurge therefore ships both mechanisms:

**The Unison module (Splurge-native).** A **voice-context multiplier**, not an audio processor. Placed in a lane's per-voice phase, it multiplies each incoming voice into N clones, perturbing each clone's voice context: pitch offset (detune amount and spread curve), pan, phase seed, level (center/side blend), optionally start-time jitter.

- **Before the generators** (the classic position): each note becomes N voice contexts; every generator in the lane instances per context with detuned pitch. Works uniformly on every source — wavetable, sampler, granular, and oscillators with no internal unison of their own (e.g., Twist gains unison for free).
- **Mid-chain:** modules before it run at the incoming granularity; modules after it run per clone, with the upstream signal broadcast into the clones — N context-varied filter instances fed from one source; ensemble effects built from first principles.
- **Stacked:** Unison modules in series multiply (3 × 3 = 9 clones per note). "Unisons of unisons" fall out for free, because the module is just multiplication on the voice set.
- Clones extend the voice lineage (note → unison group → clone), which is what keeps per-note vs. per-voice collapse well-defined (§7.4).
- **Cost:** engine-level unison clones the full per-voice phase (all generators and per-voice modules), so it is more expensive than oscillator-internal unison. This sits within the accepted cost envelope (§8.3), and the SIMD voice batcher benefits — clones fill out 4-wide batches.

**Internal oscillator unison (lifted, kept).** The lifted oscillators retain their built-in unison parameters: the cheap path (clones only the oscillator core, not the voice chain) and the **import target** — Surge patches with per-oscillator unison (e.g., OSC 1 at 7 voices, OSC 2 at 1) map directly onto these parameters with zero translation (§12.3). The UI presents the two coherently — built-in unison as a generator's "classic" unison control; the Unison module as the structural tool — so they don't read as competing features.

**Engine foresight:** voice contexts carry their lineage and per-clone perturbations (pitch offset, pan, phase seed, level) as first-class data readable by the engine and by modules. This same data is required by per-note collapse and key-tracked modulation, so it is part of the v1 voice-runtime design regardless of when the Unison module itself ships (§13).

---

## 8. Performance Architecture

### 8.1 SIMD voice batching, generalized

Surge's core DSP optimization processes 4 voices simultaneously through the same filter algorithm, with per-voice state packed into adjacent 128-bit SIMD lanes; more than 4 voices are handled as sequential 4-wide batches. Splurge **keeps this optimization and generalizes it**: in a lane's per-voice phase, the engine batches *any* voice-batchable module across voices — filters, waveshapers, and whatever else qualifies — not just a privileged filter pair. A module qualifies for batching if its algorithm is branchless (or branches in lockstep), vectorizable, and its per-voice state packs into SIMD lanes; this is exactly the per-voice context capability of §6.3.

### 8.2 Thread-level parallelism at the lane grain

The design conclusion on multicore use:

- **Lane-grain parallelism is the right grain.** Lanes that don't depend on each other are independent until they merge; the lane graph is a DAG (guaranteed acyclic by the numbered-lane rule), so a topological scheduler can dispatch independent lanes to a worker pool and synchronize only at merge points, once per block.
- Mechanism: a **persistent worker pool** (never spawn threads in the audio callback) fed by a lock-free task queue; **per-worker accumulation buffers** with a final reduction to avoid write contention; platform real-time scheduling cooperation where available (e.g., macOS audio workgroups).
- **Voice-batch-grain threading was considered and rejected**: the work units are microseconds at typical block sizes, so synchronization overhead rivals the work; hosts already parallelize across plugin instances.
- **SIMD widening to 8 lanes (AVX) was considered and rejected as a strategic move**: it is x86-only (NEON is 128-bit), forcing dual implementations for a non-portable ~2× in one pipeline stage. It remains a possible later optimization, not an architecture driver.
- The worker pool is **deferable**: v1 may ship single-threaded, but the lane graph must be designed as a parallelizable DAG from the start so threading is an addition, not a refactor. A user-facing "use up to N cores" control is a possible later refinement.

### 8.3 Acceptable cost envelope

A modest increase in per-voice CPU overhead is acceptable in exchange for architectural generality and better multi-core utilization; what matters is wall-clock time per block and the maximum sustainable voice/lane count.

---

## 9. Modulation System

### 9.1 Identity: dynamic IDs and multi-output sources

- Modulation sources are identified by **stable string/hash IDs**, not enum integers (Surge's `ms_lfo1`-style enum does not carry over). Counts are not fixed: arbitrary numbers of modulators per lane and per patch.
- **Multi-output modulators are first-class.** A routing source is the tuple *(source ID, output name)*, and each modulator declares its outputs with semantic names. This preserves and generalizes Surge's indexed-output convention (MSEG and Formula modulators already emit up to 8 outputs) and is required for planned modulators such as the spiral LFO with separate X and Y outputs (§13).

### 9.2 Scopes

- **Voice scope** — one modulator instance per sounding voice (envelopes, voice LFOs); state lives on the voice; instantiated at note-on.
- **Lane scope** — one instance shared by all voices in the lane (Surge's "scene LFOs" map here); runs once per block.
- **Patch scope** — macros and global sources (mod wheel, aftertouch, MIDI CC, pitch bend, key-tracking sources), addressable from any lane.
- **Macros are modulatable** (meta-modulation): a macro's output is its base value plus a modulation underlayer, so LFOs/envelopes/other macros can modulate macros. This capability exists in Surge and is preserved.
- Scope is a property on the modulator; the cross-phase constraint of §6.4 applies at the voice→post-mix boundary.

### 9.3 Modules expose internal signals as routing sources

Beyond declaring parameters that *receive* modulation, modules declare signals they *emit*. Oscillators expose at minimum: **audio output**, **effective pitch**, **effective frequency (Hz)**, **gate**, and **retrigger**. LFOs expose their value output(s). This single principle makes the following fall out as ordinary routing rather than special-case features: oscillator-as-modulator, ratio/harmonic pitch modes (§13), key-tracked modulation, and cross-module pitch effects.

### 9.4 Audio-rate modulation: the hybrid approach

Three options were evaluated for letting oscillators modulate parameters:

1. Downsample oscillator output to control rate — rejected (cannot actually do FM/PM).
2. Make every parameter an audio-rate buffer — rejected (a massive rewrite of every consumer).
3. **Hybrid (chosen):** routings whose source is audio-rate and whose destination is on an **allowlist of audio-rate-capable destinations** get a dedicated fast path that hands the audio buffer directly to the consumer; all other routings remain control-rate (evaluated once per block). The initial allowlist: the four oscillator destination ports (PM, linear FM, exponential FM, AM — §9.5), filter cutoff, and **delay time** (delay lines already interpolate their read heads, so module-side support is cheap; validated by the phase-distortion scenario in Appendix B.2) — growable over time.

### 9.5 Oscillator-as-modulator: four destination ports

Oscillators in Splurge expose **four distinct audio-rate modulation destination ports** rather than a single "FM" input, matching Phase Plant's model. Users wire the modulator's audio output to whichever port produces the timbral character they want:

- **PM (phase modulation)** — adds the modulator signal to the carrier's running phase. The Surge-style "FM" implementation is actually PM; this is the most common port for "FM synth" sounds.
- **Linear FM** — adds the modulator signal to the carrier's instantaneous frequency. Brighter than PM at small depths; integrates pitch over time.
- **Exponential FM** — modulates the carrier's pitch in log-frequency. The analog-FM character (think CV-controlled VCOs); musical-interval feel rather than linear-Hz feel.
- **AM (audio-rate amplitude modulation)** — multiplies the carrier's output by the modulator signal (typically as 1 + depth × signal). Generates sidebands; the boundary between AM and ring-mod is a depth/offset choice.

These are individual routing destinations on the oscillator module. The UI may surface them as four labeled ports the user drags routings into, or as a context menu when dragging onto an oscillator. Each port has its own depth parameter and (per §9.10) its own routing-edge shaping. No FM-topology enum exists.

**Same-lane (v1):** oscillators in the same lane share voice scope; routing oscillator B's audio into any of oscillator A's four destination ports is a per-voice, audio-rate routing edge resolved at voice instantiation. The lifted modules' existing audio-rate FM input facility (`assign_fm`) supports PM directly; linear-FM, exponential-FM, and AM ports require small additions to the lifted oscillator API during the lift.

**Cross-lane (post-v1):** lanes don't share voices, so the only coherent semantic is the upstream lane's *summed* output feeding a downstream port as a single shared (non-per-voice) signal — closer to sidechain modulation. Deferred, and labeled distinctly ("global") in the UI when built.

> **Validation note (Appendix B):** the AM port earns its place in B.1 — gating a noise generator open only near another oscillator's waveform peaks is a three-primitive recipe (exposed audio signal → shaped edge → AM port) rather than a feature. The ports also define where Surge's phase-modulation-labeled-FM lands on import (B.3, §12.5).

### 9.6 No FM-topology enum

Splurge has no FM topology selector. Surge's four `fm_routing` configurations are entirely an importer concern; they expand into edges connecting oscillators' audio outputs to their PM destination ports (§12.5).

### 9.7 Modulator catalogue and required behaviors

Lifted: the Surge LFO (full shape set), MSEG, Formula (Lua), DAHDSR-style envelopes, step sequencer, random/alternate sources, MIDI-derived sources. Required behaviors for LFO-class modulators (mostly module-level work; listed here because they are commitments):

- **Trigger modes:** free-running (tempo-synced to host BPM when sync is enabled) or key-triggered.
- **Loop modes:** normal, ping-pong, reverse, and no-loop (one-shot/envelope mode).
- **Freely controllable phase** (start phase/offset as a parameter).
- **Modulatable output level:** each modulator's output level is itself an ordinary modulatable parameter — this is the committed mechanism for "modulation of modulation depth" cases such as vibrato that develops over time (Appendix B.5), pending the broader edge-depth question (§17 #17).

A planned extension to the catalogue (not lifted from Surge): **audio-input modulators** — modulators that *consume* audio and emit control signals, beginning with an envelope follower. The modulator API reserves audio-input ports from v1 so this lands as an additive module (§13; motivating cases in Appendix B.4 and B.12).

### 9.8 Extending the modulator roster

Two patterns, chosen by fit: a variation that fits the existing LFO's parameter schema becomes a **new shape** in the lifted LFO; fundamentally new behavior with its own state model and parameters becomes a **new module type** under a new ID (§10.3). The spiral LFO and chaotic-attractor modulators are the latter.

### 9.9 Effective values and range data (the UI contract)

The modulation system maintains, per parameter:

- The **current effective value** (base value plus all modulation contributions, as actually applied this block).
- The **modulation range list**: each routing's contribution interval and its source type/identity — sufficient for the UI to draw a composite, color-coded range and a value cursor.

This data is produced on the audio side where modulation is applied, cached, and read by the UI; parameters carry **dirty flags** so the UI repaints only what changed (§14.2). This is new infrastructure — Surge has no "what is my total modulation range" query; the design review confirmed it must be added, and that recomputing it per UI frame by walking routing lists would be too costly.

### 9.10 Routing-edge shaping

Every modulation routing edge — not just specific source types — carries an optional **shaping function** applied between the source's value and the destination's parameter. Defaults to a linear mapping; selectable shapes include linear, exponential (weighted toward either end), S-curve, custom-curve (a user-editable 2D curve), step/quantized, and inversion. This is a routing-level feature, not a modulator-level one: the same LFO modulating two destinations can use a linear curve into one and an exponential curve into the other.

This covers the "custom pitch-bend response curve" goal — the pitch-bend wheel is a routing source, the destination is any pitch-modulatable parameter, and the response curve is the routing's shaping function. Users wanting non-linear pitch bend simply pick a non-linear shape on that routing edge. It also generalizes to any modulator behavior that would otherwise require a special-case "response" parameter (velocity curves, aftertouch curves, key-tracking curves, etc.) — they all become the routing edge's shaping function.

Audio-rate routings (§9.4) support shaping where the engine can apply it sample-accurately; control-rate routings always do.

> **Validation notes (Appendix B):** edge shaping is the decisive ingredient in B.1 (a steep threshold curve turns continuous AM into peak-gating) and B.2 (the inversion shape gives the stereo delay its anti-phase L/R modulation from a single noise source). Both tricks would otherwise require special-case features; with shaping on the edge they are recipes.

### 9.11 Globals-as-defaults policy (per Principle 2)

Where Splurge has chosen per-unit ownership over a setting that Surge made global or scene-global, two ergonomic concerns arise: (a) users sometimes want one knob that affects many units uniformly; (b) per-unit storage is more verbose. The pattern:

- **Default mode (preferred):** the patch carries an optional **default value** for the setting. Each per-unit instance has an "inherit / override" toggle that defaults to inherit. Changing the patch-level default propagates to all inheriting units; explicitly setting a per-unit value detaches it from inheritance. This is bidirectional: the UI presents a "promote to default" action so a per-unit value can become the new default.
- **No silent globals:** there is never a single hidden value that overrides per-unit ones without the user's explicit consent. Inheritance is visible in the UI.

This pattern applies to settings whose user intent is "set this for everything, but allow exceptions" — likely candidates include the inherited Character per oscillator, polyphony/portamento defaults inherited by new lanes, default tag for tagging behavior, and similar. Settings whose intent is truly singular (master tuning, master output gain, MIDI channel count) remain plain patch-level values without the inheritance mechanism. The list of which settings use which pattern is an open item for the engine epic (§17).

---

## 10. Module Library

### 10.1 Inventory of lifted modules

- **Oscillators (12):** Classic, Sine, Wavetable, S&H Noise, Audio Input, FM3, FM2, Window, Modern, String, Twist, Alias.
- **Effects (~31):** delays (Delay, Floaty Delay), EQs (3-band parametric, 11-band graphic), modulation effects (Phaser, Flanger, Chorus, Ring Modulator), spectral/spatial (Frequency Shifter, Rotary Speaker, Mid-Side Tool), tone/dynamics (Distortion, Waveshaper, Conditioner, Resonator), reverbs (Reverb 1, Reverb 2, Spring Reverb), character/experimental (Vocoder, Combulator, Treemonster, Ensemble, Tape, Bonsai, Nimbus, CHOW, Neuron, Exciter), Convolution, Audio Input, and the **Airwindows wrapper** (~70+ algorithms behind one selector).
- **Filters:** Surge's per-voice filter algorithm library (the SIMD-vectorized implementations), lifted as individual filter modules.
- **Waveshapers:** the 40+-entry transfer-function library (§10.5).
- **Modulators:** per §9.7.
- **Infrastructure:** wavetable structures and I/O, sample loading, MTS-ESP/native tuning, DSP math, oversampling filters, smoothers.

### 10.2 Module identity and namespaces

Every module type has a **stable string ID**, namespaced by origin: lifted modules as `surge.*` (e.g., `surge.sine`, `surge.wavetable`), Splurge originals as `splurge.*` (e.g., `splurge.sampler`, `splurge.granular`, `splurge.spiral-lfo`). Splurge patches reference modules by these IDs. Loading a patch that references an unknown ID fails cleanly and informatively.

### 10.3 Evolution and compatibility rules

These rules guarantee that import compatibility survives indefinite library growth:

| Change | Import-safe? | Rule |
|---|---|---|
| Add a new module type | Always | New IDs live in their own namespace; old presets never reference them |
| Add a new modulation source or destination | Always | Same reasoning |
| Add a parameter to an existing lifted module | Yes | Must ship a sensible default for old presets |
| Add a shape/mode/sub-algorithm to an existing module | Yes | Old presets simply don't select it |
| Rename a parameter's display name | Yes | Display names are not identity |
| Rename a parameter's ID | No | Forbidden without a migration shim mapping old → new |
| Change a parameter's range or units | Only with migration | A converter for stored values is mandatory |
| Remove a lifted module | Never | Deprecate and hide from menus instead; the registry keeps it forever |
| Change a module's sound for the same parameter values | Only with versioning | Bump that module's streaming version and keep old behavior reachable; when in doubt, **fork under a new ID** rather than mutate |

Additionally: the **Surge-enum → string-ID translation table is frozen forever** once shipped (§12.3), and lifted-module parameter schemas are **append-only**.

### 10.4 Per-module streaming versions

Surge's pattern of per-module streaming-version fix-ups (each oscillator/effect reconciles old patch data against current behavior at load) is **kept intact in the lifted modules** — inheriting Surge's accumulated migration logic for free — and **adopted for Splurge's own modules** going forward, so Splurge presets stay loadable across Splurge versions by the same mechanism.

### 10.5 Primitives and packaged effects (the waveshaper/distortion ruling)

A waveshaper is a memoryless per-sample transfer function; a distortion effect is that kernel **plus context** (drive, pre-filter, post-filter, output gain, wet/dry, optionally internal oversampling). Functionally a distortion at neutral settings *can* act as a bare waveshaper, but Splurge ships **both as separate modules**:

- **Waveshaper (primitive):** the transfer function with drive/output gain; carries Surge's full 40+-shape library; cheap enough to drop anywhere, including the per-voice phase.
- **Distortion (packaged):** pre-gain → pre-filter → the same shape library → post-filter → mix/output, with optional oversampling; the successor to Surge's Distortion effect.

The general principle (Principle 4) extends across the library — filter primitive vs. auto-filter package, delay-line vs. chorus/flanger packages — whenever a kernel is independently useful.

**Splurge-native utility primitives** added to support the lane model and the importer:

- **Ring modulator** — multiplies two audio inputs. Used by patches that want ring as a sound, and by the importer for Surge's `fc_ring` filter mode (§12.4).
- **Stereo width** — controls stereo image. Used in patches and by the importer for Surge's `fc_wide` filter mode (§12.4).
- **Mixer/Mix utility** — mixes N audio inputs with per-input level and pan; used inside oscillator-bearing lanes (replaces Surge's scene mixer) and as a general primitive.
- **Soft-clip** — soft-clipping primitive available as a module on feedback paths and where users want explicit soft-clipping (used internally by feedback-capable modules; also exposed for direct use).
- **Noise generator** — multi-color noise (white, pink, brown, blue, violet, gray); replaces Surge's per-scene noise channel and is broadly useful.
- **Unison** — the voice-context multiplier of §7.5: clones voices with seeded variations (detune, pan, phase, level); stackable; placeable before generators or mid-chain.
- **Channel splitter / M-S** — splits a stereo signal into per-channel (L/R) or mid/side outputs routed to separate lanes, recombined downstream; the channel-axis sibling of the multiband splitter. Enables dual-mono and mid/side processing chains.

These are independently useful and not import-only artifacts.

### 10.6 Effects taxonomy and crediting

- Surge's "multieffects"-style silos are **dissolved**: all effects live in one unified, categorized menu (filtering, delay, reverb, distortion, dynamics, modulation, spectral, utility, …).
- Effects derived from external projects are **credited inline** in their menu entries — e.g., "ZLowpass [AW]" or "ZLowpass [Airwindows]" under the Filtering category, or under an Airwindows subheading within their functional category. Individual Airwindows algorithms are **hoisted into the master menu** by function rather than buried behind a single "Airwindows" effect entry (the single-wrapper dispatch implementation may remain; the *organization* changes).

### 10.7 Surge globals reshaped per-unit (per Principle 2)

The dependency audit found several Surge "global" or scene-wide settings that conceptually belong to modules. They are reshaped per-unit in Splurge:

| Surge concept | Where it lived in Surge | Splurge form |
|---|---|---|
| **Character** (warm/neutral/bright) | Global patch parameter affecting all oscillators uniformly | A parameter on each oscillator module (default value optionally inherited from a patch-level default per §9.11). Importer fans the global Surge value out to all imported oscillators. |
| **Scene volume / VCA level** | Per-scene parameter | A parameter on the lane (the lane's output gain). |
| **Scene filter feedback** | Per-scene parameter on the filter block | The feedback routing edge (§6.6); depth is the feedback amount. |
| **Scene lowcut** | Per-scene highpass filter | A regular highpass filter module in the lane, like any other filter. |
| **Scene mute/solo per source** (per-oscillator and per-ring-mod and per-noise) | Mixer-level flags | Each oscillator (and noise generator, ring-mod) is its own module with its own enable; the lane's mixer module (§10.5) carries mute/solo per input. |
| **Scene FM switch** | Per-scene boolean | Each PM/FM/AM routing edge can be muted, just like any other routing. No global FM switch. |
| **Per-oscillator route (to Filter 1 / Both / Filter 2)** | Per-oscillator scene-mixer parameter | The lane's routing graph (each oscillator connects to whichever downstream module the user wires). |
| **Per-scene polyphony mode / portamento / key range / channel filter** | Scene-level | Per-lane (already established in §7.1). |

Settings that legitimately remain patch-wide:

- Master tuning (the scale/keyboard file).
- Master output gain (the final master-bus level).
- MIDI configuration that doesn't sensibly vary per lane.

The §9.11 globals-as-defaults mechanism is available for any per-unit setting that benefits from a patch-level convenience default; the engine epic decides which.

---

## 11. Patch Format & Persistence

### 11.1 Format: structured, machine-readable, programmatically authorable

Per Principle 11, the patch format must be straightforward for tools, scripts, and language models to compose — not just for the GUI to read/write. **Decision: JSON.** Rationale:

- Universal tooling in every language; LLMs produce well-formed JSON reliably.
- Schema-validatable (JSON Schema), enabling editor support, generation, and round-trip testing.
- Sufficiently human-readable for debugging (whereas a binary format isn't).
- TOML was considered but rejected: less universally supported for deep nesting, weaker LLM facility, less mature schema ecosystem.

The patch is a **JSON document** with a defined schema. Binary resources (samples, wavetables, impulses) are **addressed by content hash** within the JSON and stored as a separate sidecar — either alongside the JSON file (`patch.json` + `patch.resources/`) or packaged as a single bundle (a ZIP, opaque to the user but containing both). The bundle layout keeps human-editable patches viable while not embedding multi-megabyte sample data in JSON strings.

**Clarification on "human-readable":** the requirement is that the patch body is *not an opaque binary blob* — it is inspectable, diffable, debuggable text. It does **not** mean the schema must be simple. The JSON may be as deeply structured and as complicated as is reasonable in service of the forward-compatibility contract (§11.6); flexibility and machine-composability take precedence over hand-editing ergonomics.

- **Versioned from day one**, with Splurge's stream revisions starting **above** Surge's range (e.g., at 100, with Surge XT currently at 28): any patch whose revision is below the Splurge floor is routed to the Surge importer; at or above, it is a native Splurge patch.
- Splurge presets are explicitly **not** backward-compatible with Surge (§1.3).
- Splurge does not load Surge `.fxp` files into the same code path as native `.splurge` patches; the importer (§12) produces a Splurge in-memory model that is then serialized as Splurge JSON.

### 11.2 Resources

Resources are a **generic, typed-blob** facility addressed by content hash (e.g., SHA-256), not wavetable-specific. The JSON references resources by hash; the loader resolves hashes against the patch's sidecar/bundle, then against a user-configured library. Required so samples (sampler oscillator), wavetables, impulses (convolution), and any future binary resource type embed uniformly. By-name references to library resources are also supported (and preferred where possible — content-hash resolution preserves a name-resolution fallback for portability).

> **Validation note (Appendix B):** the resource facility is also the landing zone for the **resample-to-wavetable** wishlist feature (B.3) — a captured wavetable is just another content-hashed resource the engine writes instead of reads, which is why that feature is architecture-ready despite not being scheduled.

### 11.3 Metadata, categories, and tags

- Fields: name, author, license, comment, category, **tags** (multiple), optional description, optional MIDI program metadata.
- **Tags are first-class.** (Design review finding: Surge already serializes tags in patch metadata and indexes them in its SQLite patch database — the gap is purely UI. Splurge surfaces them fully: tag editing, tag filtering, tag-based browsing.)
- **Open investigation:** collapsing categories into tags entirely (category as a distinguished tag) — see §17.

### 11.4 Library organization

- **One unified factory tree, organized by sound category** (Basses, Leads, Pads, …). The Surge-style split — factory content by category but third-party content grouped by author folder — is eliminated: every preset, including third-party content, lives under a category, with the **author credited inline** in the browser (read from patch metadata, which Surge patches already carry).
- User content remains in a user location, also categorized and tagged.

### 11.5 Programmatic composition use cases

The JSON format is required to support, without privileged tooling:

- A user or tool composing patches in a text editor.
- A language model generating patches from a natural-language prompt.
- A scripting environment (e.g., a CLI tool or Python wrapper) procedurally generating patch families.
- Round-trip transformations (load, edit, save) that preserve everything the loader didn't understand (forward-compatible passthrough of unknown fields where safe).

The engine epic must define the JSON Schema and ship a schema-validation step on load that produces actionable errors.

### 11.6 The compatibility contract and schema-evolution rules

**The contract, stated unambiguously in both directions:**

- **Forward (hard guarantee):** a preset saved by *any* Splurge version loads correctly in *every later* Splurge version, forever. There is no expiry, no "supported window," no exceptions.
- **Backward (explicitly not promised):** a preset saved by a newer Splurge may not load in an older Splurge. When an older version encounters a newer schema revision, it **refuses cleanly** with a clear message naming the required version — it never crashes and never half-loads silently.

The guarantee is achievable only if the schema is engineered for evolution from day one. The following rules are binding on the schema design (and justify whatever structural complexity they require):

1. **Self-describing, ID-addressed structures.** Modules, modulators, lanes, routings, and resources are typed JSON objects identified by stable string IDs — module *type* IDs per §10.2, plus instance IDs unique within the patch. **Nothing is identified by array position**; inserting, removing, or reordering objects never changes the meaning of anything else.
2. **Per-object version stamps.** The document carries a schema revision; each module instance additionally carries its module streaming version (§10.4). Loaders run migration chains *per object*, not merely per document — a patch can contain modules saved at different module versions and each migrates independently.
3. **Additive-only evolution.** New features appear as new keys and new typed objects. Existing keys are never repurposed and never change meaning. Removal is deprecation: loaders understand removed concepts forever, even if only to migrate them into their replacements.
4. **Explicit defaults everywhere.** Every field has a documented default; absence is always legal and always means the default. A parameter added to a module in version N+1 loads version-N instances at its documented default (§10.3).
5. **Open extension points.** Every object tolerates unknown keys without error. A reserved extension namespace (e.g., a per-object `extensions` map) exists for experimental data that may later be promoted into the schema proper.
6. **Unknown-content policy, by case.** (a) Unknown keys in a same-or-older-revision document: preserved and re-serialized on save (round-trip passthrough) where safe — supporting external tools that annotate patches. (b) A higher-revision document: clean refusal per the contract. (c) An unknown module *type* ID: a clean, specific error naming the missing module (§10.2) — distinguishable from schema failure, since it may simply mean a missing optional module pack.
7. **Migrations are forever.** Once a migration step ships, it is never deleted. The loader migrates any historical revision stepwise to current. The test suite retains golden presets from **every shipped revision** and loads them all on every release (§15).
8. **No semantic magic numbers.** Enums serialize as strings, not integers (a reordered enum can never corrupt a patch). Units and value semantics are explicit in the schema, never implied by context that could drift between versions.

These rules exist precisely so that features not yet conceived — new module types, new routing concepts, new bus types, new lane semantics, things this document doesn't anticipate — land as *additive* schema (new typed objects, new keys, new extension data) without ever breaking the ability to read what came before.

---

## 12. Surge XT Import

### 12.1 Philosophy

One-way translation, Surge XT → Splurge. Success criterion: **perceptual audio equivalence** (not bit-identity) for imported presets. The importer is the *only* component (outside the lifted modules themselves) allowed to contain Surge-specific knowledge, and it is allowed to be elaborate; restructuring (splitting, merging, reshaping topologies) is fine as long as the sound survives. There is no concept of "scene" in Splurge — the importer constructs Splurge lanes from each populated Surge scene and discards the scene container entirely.

### 12.2 Why this is tractable

1. Surge patches are a binary wrapper around readable XML — hand-debuggable.
2. Because the lifted modules keep their parameter-bag structs (§3.2), every Surge parameter value populates the identical structure the Splurge module reads — no per-parameter reinterpretation.
3. Surge's fixed topology is a strict subset of what lanes can express, so import is a well-defined projection.
4. Keeping the lifted modules' streaming-mismatch logic (§10.4) means patches from *any* Surge era are first normalized by Surge's own accumulated migration code.

### 12.3 The structural mapping (per Surge scene)

For each populated Surge scene (typically two — scene A and scene B), the importer constructs one Splurge lane. The lane is named after the scene index ("Scene A" / "Scene B") for user familiarity but carries no special status.

| Surge concept | Splurge mapping |
|---|---|
| Per-scene polyphony mode (poly / mono / mono_st / mono_fp / mono_st_fp / latch) | Lane voice configuration |
| Per-scene key range and MIDI channel filter (derived from `scene_mode`: single / split / dual / chsplit, with `splitpoint`) | Lane key-range and channel-filter parameters |
| Per-scene portamento + its sub-options (curve: log/lin/exp; constant rate; glissando; retrigger) | Lane portamento parameters; all sub-options preserved as per-parameter flags (§12.7) |
| Per-scene 3 oscillators | 3 oscillator module instances in the lane, in order |
| Per-oscillator unison (count, detune, extended/absolute flags) | The lifted oscillator's **internal** unison parameters, unchanged (§7.5); the importer never synthesizes a Unison module |
| Per-oscillator level / mute / solo | Carried as parameters on the lane's mixer module (§10.5) |
| Per-oscillator route (Filter 1 / Both / Filter 2) | Wires from each oscillator to either or both filter modules in the lane chain — implemented via the lane's mixer routing, not Surge-style enums |
| Ring-modulator 1×2 and 2×3 channels (each with level / mute / solo / route) | Two ring-mod modules (§10.5) in the lane, each consuming the appropriate oscillator pair, with routes wired per the route parameter |
| Noise generator (level / color / route / mute / solo) | A noise-generator module (§10.5) in the lane, routed per its route parameter |
| Scene VCA level | Lane output gain parameter |
| Per-scene 2 filter units (with type, cutoff, resonance, etc.) | Two filter modules in the lane |
| Per-scene filter routing mode (`fc_serial1/2/3`, `fc_dual1/2`, `fc_stereo`, `fc_ring`, `fc_wide`) | Module arrangement + feedback routing + utility modules per §12.4 |
| Per-scene filter feedback (soft-clipped) | A feedback routing edge from the lane's mid/post point back to the first filter's feedback input (§6.6), with the Surge feedback amount as the routing depth |
| Per-scene filter balance | The lane's mixer module level/blend between filter outputs |
| Per-scene lowcut (highpass, deactivatable) | A highpass filter module placed appropriately in the lane chain (or absent when deactivated) |
| Per-scene waveshaper (type + drive + position per filter config) | A waveshaper module placed in the lane chain per the filter config's waveshaper position |
| Per-scene FM routing (`fm_off` / `fm_2to1` / `fm_3to2to1` / `fm_2and3to1`) and FM depth | Routing edges into oscillator PM destination ports (§12.5) |
| Scene insert FX (4 per scene) | The lane's post-mix (phase 2) effects, in order |
| Per-scene FX bypass mode (all / no sends / no send+global / no FX) | A patch-level bypass setting or per-lane equivalent — TBD by engine epic |
| Send slot levels (4 per scene) | Oscillator-free lanes (Splurge send-bus lanes) receiving weighted input from the scene lanes; one Surge send slot = one Splurge bus lane |
| Global FX (4) | The master output's effect chain (the master path has its own lane in Splurge) |
| Per-FX `return_level` | Carried as the per-effect mix/output parameter |
| **Character** (warm/neutral/bright) — global patch parameter | Fanned out: each imported oscillator's Character parameter is set to the imported value (§10.7) |
| 8 macros (custom controllers) | 8 patch-level macros with meta-modulation preserved |
| 6 voice LFOs + 6 scene LFOs per scene | 12 modulators in that lane: 6 voice-scoped, 6 lane-scoped |
| Modulation routings (voice / scene / global lists) | Routing entries with translated source identities, via a **static, frozen lookup table** (~40 entries) from Surge's source enum to Splurge IDs, including indexed outputs (MSEG/Formula output indices → output names) |
| MSEG, Formula, step-sequencer data | Lifted storage structures, unchanged |
| Wavetables (embedded or by name) | Same mechanisms, via the generic resource facility (§11.2), with embedded wavetables added as content-hashed resources |
| Tags, author, comment, license, category | Carried into Splurge metadata; category re-homed into the unified tree |
| Streaming revision (≤ 28) | Triggers the importer (§11.1) |

### 12.4 Filter-config mapping (all 8 configs, with feedback)

Surge's `filter_config` enum has eight values, differing chiefly by **feedback topology** (not by waveshaper position as the earlier draft claimed) and by stereo/ring arrangement. The mapping into Splurge primitives:

| Surge `filter_config` | Feedback path | Filter arrangement | Waveshaper placement | Splurge import shape |
|---|---|---|---|---|
| **fc_serial1** | None | Filter 1 → Filter 2 | Between filters (when active) | Two filter modules in series; waveshaper in between if active. No feedback edge. |
| **fc_serial2** | From final output, soft-clipped, summed into input | Filter 1 → Filter 2 | Between filters | Same series chain; **feedback edge** from after Filter 2 back to Filter 1's feedback input, with soft-clip on the path. |
| **fc_serial3** | From Filter 2 only, soft-clipped, summed into input | Filter 1 → branch: continue to output **and** to Filter 2 → Filter 2 only feeds feedback | Between Filter 1 and the branch | Series with a feedback edge sourcing from Filter 2's output (a different tap than serial2). Good for physical-modeling patches. |
| **fc_dual1** | From final output | Filter 1 ‖ Filter 2 (parallel), then mixed | After mix | Sub-lane split: one filter per sub-lane; recombine via mixer module; waveshaper after; feedback edge from output to input. |
| **fc_dual2** | From final output | Filter 1 → waveshaper → Filter 2 (with secondary path) | Inside the dual chain | Series with a parallel branch; waveshaper position varies. Use sub-lane split if necessary. |
| **fc_stereo** | Per-channel, from each channel's output | Filter 1 on L only, Filter 2 on R only | Per-channel | Lane uses stereo channel layout (§5.4); Filter 1 on L, Filter 2 on R; per-channel waveshaper and feedback. |
| **fc_ring** | From final output | Filter 1 ‖ Filter 2, then **ring-modulated** via the mixer's blend params | After ring | Sub-lane split with ring-mod combine module (§10.5); waveshaper after; feedback edge. |
| **fc_wide** | Per-channel | Four filter instances (L1, R1, L2, R2) — Filter 1 and 2 each duplicated for L/R | Per-channel | Stereo channel layout with **four** filter module instances and per-channel waveshapers; stereo-width module after; per-channel feedback edges. Amplitude correction (×0.6667) applied via the lane output gain. |

Notes:

- The feedback amount, when present, comes from Surge's per-scene `feedback` parameter (with extended-range support).
- The soft-clip on feedback paths uses the soft-clip module (§10.5); the importer inserts it explicitly on the feedback routing edge.
- The waveshaper position is per-config; the importer follows the topology above. (Earlier drafts of this doc incorrectly stated that serial 1/2/3 differ by waveshaper position; the actual distinction is feedback topology. The waveshaper position varies separately per config.)
- `fc_wide` instantiates **four** filter modules in the Splurge lane, not two. This is the import shape; the user can simplify after import.

### 12.5 FM-routing mapping (corrected)

Surge's four `fm_routing` configurations import as edges into oscillator PM destination ports (§9.5). Note: Surge's "FM" is implemented as phase modulation with cubic depth scaling, hence the **PM** port:

| Surge `fm_routing` | Imported routing edges |
|---|---|
| `fm_off` | none |
| `fm_2to1` | OSC 2 audio → OSC 1 PM port |
| `fm_3to2to1` | OSC 3 → OSC 2 PM port; OSC 2 → OSC 1 PM port |
| `fm_2and3to1` | OSC 2 → OSC 1 PM port; OSC 3 → OSC 1 PM port (**both** modulators target carrier 1) |

The PM depth is taken from Surge's `fm_depth` parameter; the importer applies Surge's cubic scaling so the perceived depth matches. Splurge users can subsequently re-wire the edges to linear-FM, exp-FM, or AM ports (§9.5) if they want different timbral character.

### 12.6 Scene mode → lane filtering

Surge's `scene_mode` enum determines how the two scene lanes consume incoming MIDI. The mapping:

| Surge `scene_mode` | Splurge lane configuration |
|---|---|
| **sm_single** | Only Scene A's lane is created; or both are created and Scene B's key range is set to empty. |
| **sm_split** (with `splitpoint`) | Scene A's lane key-range upper-bounded at splitpoint; Scene B's lane key-range lower-bounded at splitpoint. |
| **sm_dual** | Both lanes carry the same key range; both receive every note (layering). |
| **sm_chsplit** | Scene A and Scene B lanes filter on different MIDI channels per Surge's channel-split convention. |

The MPE channel convention from Surge maps onto these lane filters by default; users can re-edit.

### 12.7 Per-parameter flag preservation

Surge's `Parameter` struct carries per-parameter flags that materially affect behavior; the importer must carry every one into the corresponding Splurge parameter:

- **`temposync`** — the parameter is locked to host BPM (rates, delays, LFO speeds).
- **`extend_range`** — the parameter's value range is doubled (or otherwise extended); affects how the stored normalized value maps to the actual value.
- **`absolute`** — the parameter is interpreted as an absolute value rather than relative in some modulation contexts.
- **`deactivated`** — the parameter is currently inert (computed by Surge from signal flow, e.g., waveshaper inactive in a config that bypasses it). Splurge may compute this independently from its own routing or carry it explicitly.
- **`deform_type`** — selects a sub-variant of a parameter's behavior (envelope/LFO deform modes, etc.).
- Portamento sub-options on portamento parameters: `porta_curve` (log / lin / exp), `porta_constrate`, `porta_gliss`, `porta_retrigger`.

Each of these is a per-parameter attribute, not a separate parameter; the import preserves them faithfully.

### 12.8 Source translation table (frozen)

The importer maintains a **static, frozen lookup table** (~40 entries) mapping Surge's `modsources` enum (`ms_lfo1`–`ms_lfo6`, `ms_slfo1`–`ms_slfo6`, `ms_ampeg`, `ms_filtereg`, `ms_velocity`, `ms_keytrack`, etc.) to Splurge string IDs, with indexed outputs (MSEG / Formula output indices) becoming named outputs in Splurge tuple keys. Once shipped, this table is never changed; new Surge revisions extending the enum can be added forward but never reordered.

### 12.9 What survives, what's hard, what's lost

- **Preserved:** all parameter values (with flags), oscillator/effect type selections, modulation routings and depths, macros (incl. meta-modulation), wavetables, MSEG/Formula/step data, tags and metadata, Character (fanned out per-oscillator), scene-mode key/channel filtering.
- **Hard parts (bounded):** the per-config filter translations above, especially fc_wide's four-filter expansion and the feedback-path soft-clip insertion; the ~40-entry source translation table (mechanical); MPE/channel-mode mapping; macro meta-modulation (free if the modulator model allows modulators to target modulators, which it does).
- **Acceptable losses:**
  - Patches exploiting the Alias oscillator's audio-from-patch-memory modes (Principle 9; §3.3); users get an import warning and a benign Alias mode.
  - Patches relying on ODDSound MTS-ESP at load time (the protocol is not adopted; the scale/tuning portion via `tuning-library` works). Surge presets that don't actually use MTS-ESP — the vast majority — are unaffected.
  - Patches exploiting un-migrated ancient-Surge quirks or topology edge cases that don't project cleanly (expected to be vanishingly rare). Users are warned when an import is imperfect.

### 12.10 Importer build strategy

Built incrementally, each stage a regression checkpoint: (1) smoke-test import — right oscillators at right pitch, most routing ignored; (2) full parameter coverage including all per-parameter flags; (3) modulation routings via the frozen source-translation table; (4) FX chains, sends, and lane mapping; (5) filter configs and feedback paths; (6) the full factory corpus loads and sounds correct. The corpus then remains a permanent regression suite.

---

## 13. Planned Future Features and Their v1 Foresight

These are agreed *nice-to-haves* — **not** initial-release commitments — evaluated for what the v1 architecture must anticipate. Several foresight items are baked into the v1 design; everything else is purely additive later.

| Feature (inspiration) | Description | Architectural prerequisite | Status of prerequisite |
|---|---|---|---|
| Sampler oscillator (Serum 2, Phase Plant) | Sample playback as an oscillator: pitch resampling, start/end, loop points | Generic embedded binary resources in the patch format | **Baked into v1** (§11.2) |
| **Resample-to-wavetable** (Serum 2) | Render a lane's output (or a selection of it) into a framed wavetable resource, in-app; enables tricks like tempo-synced reece beating (Appendix B.3) | Offline/headless render path (already implied by the framework-independent engine); frame slicing; generic resource facility (§11.2) | Feature wishlist — architecture-ready; pure addition |
| Granular oscillator (Phase Plant) | Grain scheduling/density/position over sample data | No fixed per-voice state cap | **Baked into v1** (§7.3) |
| Spiral LFO; chaotic modulators (Serum 2; Lorenz attractors, etc.) | Modulators emitting separate named outputs (X and Y, etc.) | Multi-output routing sources | **Baked into v1** (§9.1) |
| Pitch modes: octave/semi/fine; pitch ratios (Serum 2); harmonic ratios (Phase Plant, Serum 2); pitch shift (Phase Plant) | Ratio/harmonic modes follow another oscillator's pitch | Oscillators expose effective pitch as a routing source; the mode UI is a shortcut that creates the edge | **Baked into v1** (§9.3) |
| **Multiple FM types on each oscillator (Phase Plant)** | PM, linear FM, exponential FM, AM as separate destination ports on every oscillator; users wire modulators to whichever port produces the timbre they want | Module API for multiple distinct modulation destinations per module | **Baked into v1** (§9.5) |
| Per-note lane processing (Phase Plant "Poly") | A mix point set to per-note collapse; each chord note's signal processed independently downstream | Typed buses with voice-lineage data; per-note collapse at mix points | **Baked into v1** (§5.4, §7.4) |
| **Per-unison-voice lane processing** | A mix point set to per-voice collapse; each unison clone processed independently | Same, at clone granularity | **Baked into v1** (§5.4, §7.4) |
| **Unison module (unisons of unisons)** | Voice-context multiplier; stackable (3 × 3 = 9); works on any generator incl. sampler/granular; mid-chain placement for ensemble effects | Voice lineage + per-clone perturbation data in voice contexts; module API for voice multiplication | **Foresight baked into v1** (§7.5); the module itself may land v1 or v1.x |
| Dual-mono / mid-side processing chains | Channel-splitter and M/S utilities route channels to separate lanes | Channel-axis adaptation rules (§5.4) | Pure addition (§10.5) |
| Multichannel lane output | Surround/ambisonic/other layouts between lanes | Same typed-bus abstraction (channel axis) | **Baked into v1** (§5.4) |
| **Routing-edge shaping (custom pitch-bend curves and more)** | Every modulation routing edge carries a shaping function (linear, exponential, S-curve, custom 2D curve, step, inversion); the destination's response is curvable | Routing edges are first-class objects that carry shape parameters | **Baked into v1** (§9.10) |
| **Feedback as a routable input** | Filters and feedback-capable modules expose feedback-input ports; users route signals back into them to create resonance/feedback topologies | Module API for feedback-input destinations; 1-block delay semantics | **Baked into v1** (§6.6) |
| Phase offset & randomness (Phase Plant) | Oscillator start-phase control and per-note randomization | None — oscillator parameters | Pure addition |
| Noise oscillator | Standard and exotic generated noise colors; sampled noise expected via the sampler instead | None — new module (§10.5) | Pure addition |
| LFO behaviors | Free-run w/ BPM sync or key-trigger; normal/ping-pong/reverse/no-loop (envelope); free phase | None — module-level (§9.7); cheap enough that these may land in v1 | Pure addition |
| Cross-lane oscillator modulation | Upstream lane's summed audio as a "global" modulation source | Audio-rate hybrid path (§9.4) plus a labeled global-source concept | Post-v1 (§9.5) |
| **Audio-input modulators** (envelope follower; possibly pitch detector) | Modulators that *consume* audio and emit control signals — enables sidechain pumping, auto-wah, audio-reactive modulation (Appendix B.4) | Modulators may declare audio-input ports; routing system already supports audio-rate edges | Pure addition — port concept worth reserving in the v1 modulator API |
| Cross-module hard sync | Oscillator A's phase reset by oscillator B's cycle boundary | Event-typed signal routing (§17 #18); lifted oscillators' internal sync params cover the classic case meanwhile | Post-v1; open question |
| Stutter / buffer-repeat effect | Tempo-synced buffer capture and replay | None — new module | Pure addition |
| Open-source MTS-ESP-alternative microtonality client | Real-time host-protocol microtonality (the role MTS-ESP plays in Surge) via an open-source library | Tuning system already pluggable via `tuning-library`; protocol client adds a layer | Post-v1, evaluated (§17) |

---

## 14. User Interface Design

### 14.1 The visual feedback doctrine

The defining UI commitment, stated as requirements:

1. **Every control always displays its composite modulation range** — the union of all modulation contributions affecting it — **color-coded by modulator type**, visible at all times (not only when a modulator is selected, which is Surge's behavior). The displayed range must be the *actual reachable range* of each routing (e.g., an LFO modulating a 10-second portamento by a small depth shows only that sliver, never the whole bar).
2. **Every control carries a current-value cursor** showing the effective (modulated) value within the control's range, live.
3. **Modulation-aware displays:** oscillator displays, LFO displays, and similar visualizations reflect the *effective, currently-modulated* state — fed by the running engine's effective-value snapshots — not static parameter values. (Surge's oscillator preview renders an unmodulated instance; Splurge's must not.)
4. **Universality:** anything that can possibly have a visible control reflects its state in that control's visible state. No invisible modulation.
5. **Framerate is sacred:** none of the above may degrade GUI frame rate.

### 14.2 The data plumbing behind it

Requirement 5 is met by architecture, not hope: the engine maintains the per-parameter effective-value and modulation-range cache (§9.9), populated where modulation is applied; the UI is a read-only consumer; per-parameter **dirty flags** confine repaints to changed widgets. (Design review finding: Surge's editor refreshes modulation state for *all* parameters on a global flag at 60 Hz — the always-on composite display would multiply that cost unsustainably; per-widget invalidation is mandatory, and rendering must stay cheap per widget.)

### 14.3 Knob-first control design

Splurge's UI favors **knobs over sliders**. Surge's slider-heavy design wastes screen real estate and reads as visually intimidating; most continuous parameters become knobs (with the composite ring/arc range display of §14.1), with sliders reserved for cases where they genuinely communicate better. (In Surge this distinction is skin-orientation-driven rather than widget-driven; in Splurge's fresh UI it is a deliberate design default.)

### 14.4 The lane editor

The central new UI surface: lanes with visible module chains; the automatic phase divider rendered in-lane (§6.2); editable lane names; read-only input labels (`<multi>` for merged inputs); output-target selection per lane (which is also how serial vs. parallel is expressed); creation/reordering within the numbered-lane constraint. The overall visual metaphor (rack-style strips vs. node-graph vs. paneled) is an **open question** (§17) — with the constraint that it must keep the path-of-least-resistance patch looking like a normal synth, not a patching environment.

### 14.5 Module browsing

Unified, categorized module menus (§10.6) with origin crediting and per-entry context indicators (per-voice / post-mix / both), using approachable language for *why* (terminology TBD, §17).

### 14.6 Patch browser

One factory tree by sound category; inline author display; first-class tag filtering and editing; fast search/type-ahead over an indexed patch database (Surge's SQLite-backed patch DB concept carries forward).

### 14.7 Skinning

Whether Splurge ships a skin engine (community skinnability as in Surge) or a fixed, polished theme in v1 is an **open question** (§17). The widget architecture should keep theming (colors/assets) separable regardless.

### 14.8 Accessibility

Screen-reader labeling, keyboard navigability, and parameter accessibility are designed in from the first widget. JUCE's accessibility support is part of why it was retained.

### 14.9 Editing infrastructure

Undo/redo across parameter, routing, and structural (lane/module) edits, and host automation recording, are required product features and must be considered in the engine's edit model (structural edits are harder to undo than value edits if bolted on later).

---

## 15. Quality & Testing Strategy

- **Module harness:** standalone instantiation and golden-output tests for every lifted module — built during the lift, kept forever (§3.2).
- **Import corpus:** every Surge XT factory and bundled third-party patch must load and produce sound; staged importer milestones per §12.7; perceptual-equivalence spot checks; the corpus runs as a permanent regression suite.
- **Engine tests:** lane-graph construction/ordering, phase-boundary placement, modulation application, voice lifecycle (allocation, stealing, legato/glide), typed-bus connection rules.
- **Performance:** regression benchmarks against Surge XT on comparable patches (voices/CPU); GUI frame-time budget tests with heavy modulation visualization; thread-scheduler correctness (when enabled) under sanitizers.
- **Host matrix:** the major DAWs across Windows/macOS/Linux, exercising automation, render/freeze, sample-rate changes, MPE.
- **Sound-design validation patches:** the Appendix B trick catalogue is maintained as actual patches in the test corpus; each release must keep every recipe working.
- **Schema-revision corpus:** golden presets from every shipped schema revision, all loaded on every release (the §11.6 forward-compatibility guarantee, enforced mechanically).

---

## 16. Risk Register

| Risk | Notes / mitigation |
|---|---|
| Voice-runtime edge cases (stealing, legato, glide, MPE) | Highest concentration of rewritten domain knowledge; mitigate with Surge behavior as reference, early host testing, targeted tests |
| Host-integration papercuts | Accumulates late; mitigate by standing up the plugin shell early and testing in real DAWs throughout |
| Engine-context surface drift | The shim (§3.3) is load-bearing for every lifted module; any silent dependency the audit missed becomes a runtime bug. Mitigate by building the test harness to instantiate each lifted module against the new context and golden-output testing |
| Module-lift surprises (hidden globals, storage reach-ins) | The audit found Character-as-global, Alias memory-aliasing, and the static `shaped_sinetable`; expect more. Budget explicitly; the test harness catches behavioral drift |
| Importer long tail | Staged strategy (§12.10) and corpus regression keep it incremental |
| Feedback-edge semantics | The 1-block delay on intra-lane feedback (§6.6) is unusual; needs clear UI affordance to prevent user confusion and audio-thread implementation care |
| Per-note/per-voice lane CPU cost | Carrying N voices' buffers between lanes can blow CPU; clear UI warnings and per-connection opt-in (§7.4) |
| JSON patch performance | Large patches with embedded resource manifests may load slowly; mitigate with content-hash caching and incremental loading |
| GUI performance under always-on visualization | Per-parameter dirty flags and audio-side caching are load-bearing (§14.2); benchmark early with worst-case patches |
| Threading correctness | Deferable feature (§8.2); design the DAG now, ship threads only when provable |
| Scope creep in the lane editor | The open metaphor question (§17) should be settled with prototypes before deep investment |
| Open-source microtonality protocol availability | MTS-ESP is dropped; until an open-source equivalent is selected, Splurge is file-tuning-only (§17). Could disappoint users who rely on MTS-ESP in Surge |
| Accessibility debt | Avoided by doing it from the start (§14.8) |

---

## 17. Open Questions

Decisions deliberately left open, with context:

1. **Lane editor visual metaphor** — rack-style vertical strips vs. node-graph vs. paneled layout (§14.4). Constraint: simple patches must look like a normal synth.
2. **Skin engine in v1** — community skinnability vs. fixed theme (§14.7).
3. **Module-library evolution policy** — track upstream Surge module improvements (periodic re-sync) vs. diverge freely after the lift (§3.7 mechanics included).
4. **Context-capability terminology** — the friendly wording/iconography explaining per-voice vs. post-mix module availability (§6.3).
5. **Categories collapsing into tags** — investigate making category a distinguished tag rather than a separate axis (§11.3).
6. **Worker pool in v1 or deferred** — DAG design is committed either way (§8.2).
7. **Upstream contribution mechanics** — directory/build conventions for Surge-contributable work (§3.7).
8. **Per-module wet/dry convention** — a uniform mix-control policy across packaged effects (Surge mixes per-effect and per-slot inconsistently).
9. **Engine parameter-array layout** — emulate Surge's scene-wide `pdata*` array vs. mechanically rebase oscillator indices to per-module (§3.2).
10. **Alias oscillator audio-buffer mode disposition** — drop entirely as outside-the-spirit (§3.3, Principle 9), or preserve only the audio-input mode if doing so is cheap? Other modes (scene-data, DAW extra state, step sequences) are unambiguously dropped.
11. **Open-source microtonality client** — pick a real-time host-protocol approach to replace MTS-ESP (or accept file-tuning-only for v1 and revisit). Candidates: a future open MTS-ESP-compatible client, a Splurge-native protocol, or do nothing (§4.5).
12. **Globals-as-defaults policy details** — exact list of which Splurge per-unit parameters get the inheritance mechanism (§9.11) vs. plain per-unit-only.
13. **Patch bundle vs. sidecar** — single ZIP bundle (`.splurgepatch`) vs. JSON-plus-folder sidecar layout for resources (§11.2). Bundle is friendlier for sharing; sidecar is friendlier for tool/script editing.
14. **Schema-leniency detail** — §11.6 fixes the contract (same-revision unknown keys pass through; higher-revision documents refuse cleanly). Remaining question: whether an older Splurge should offer a best-effort partial load of a slightly-newer preset behind explicit user confirmation, or always refuse.
15. **`fc_dual2` import shape** — the precise sub-lane topology; verify against test patches.
16. **Per-FX bypass mode in import** — Surge's per-scene FX bypass settings (all / no-sends / no-send+global / no-FX). Map to patch-level or per-lane equivalent (§12.3).
17. **Routing-edge depth as a modulation destination** — should every edge's depth itself be modulatable (fully general meta-modulation, but it explodes the destination space), or is modulating the source modulator's output-level parameter sufficient (simpler; covers the common "vibrato that develops over time" case — Appendix B.5)?
18. **Event-typed signals (sync/retrigger routing)** — oscillators emit gate/retrigger (§9.3), but cross-module *hard-sync* needs an event-typed destination port (phase-reset) distinct from audio and control signals (Appendix B.6). Decide whether v1's bus/routing model reserves an event signal type or whether sync stays oscillator-internal initially.

---

## 18. Decision Index

Quick reference for planners; each decision is elaborated at the cited section.

| # | Decision | Where |
|---|---|---|
| 1 | Consume Tier A (sst-\* libs, Airwindows, eurorack, tuning-library, LuaJIT) as standalone libraries; vendor Tier B (oscillators, modulation system, lipol, wavetable I/O, non-adapted effects, Parameter) as a static library; build Tier C (engine, importer, GUI, adapters) fresh | §3.1 |
| 2 | Keep module parameter-bag structs as the module API surface; emulate or rebase Surge's scene-wide parameter array (TBD per §17) | §3.2 |
| 3 | Engine-context shim provides services modules currently read from SurgeStorage (pitch tables, sample rate, sinc tables, RNG, tempo, memory pools, audio-in buffers); aligned with `sst-effects` `Config` pattern | §3.3 |
| 4 | Alias oscillator's memory-aliasing modes dropped (Principle 9); benign fallback + import warning | §3.3 |
| 5 | Stay on JUCE; confine it to UI and adapters; core stays framework-independent | §4 |
| 6 | Mobile/WASM deferred; viability recorded; `simde` already vendored | §4.3 |
| 7 | **GPL-3 license adopted; MTS-ESP not adopted (proprietary); open-source microtonality alternative TBD** | §4.5 |
| 8 | Globals → per-unit by default (Principle 2); explicit list of reshaped Surge globals | §10.7 |
| 9 | Globals-as-defaults mechanism: optional patch-level default with per-unit inherit/override | §9.11 |
| 10 | The lane is the only container; no scenes, no voice groups | §5.1 |
| 11 | Macros and global modulators live at patch level | §5.2, §9.2 |
| 12 | Cycle prevention via the higher-numbered-lane-or-master rule | §5.3 |
| 13 | Sends are just lanes; serial/parallel emerges from routing | §5.3 |
| 14 | Typed lane buses: two orthogonal axes — voice (collapse by summation) × channel (adapt by layout rules); merging the axes was considered and rejected | §5.4 |
| 15 | Multiband splitting is a lane primitive, not an effect | §5.5 |
| 16 | Default patch is a conventional subtractive lane; routing is opt-in | §5.6 |
| 17 | Lane phases: per-voice → automatic, visible voice-mix transition → post-mix | §6 |
| 18 | Modules declare context capability; unified menus with indicators | §6.3 |
| 19 | No "filter block": filters are individual modules; no filter-topology enum | §6.5 |
| 20 | **Feedback as a routable input: feedback-capable modules expose feedback-input destinations; 1-block delay; soft-clip on path** | §6.6 |
| 21 | Quad SIMD voice batching kept and generalized to all voice-batchable modules | §8.1 |
| 22 | Threading at lane grain (persistent pool, DAG scheduler); per-voice threading and AVX widening rejected | §8.2 |
| 23 | Dynamic string IDs; multi-output sources as (source, output) tuples; no fixed modulator counts | §9.1 |
| 24 | Modulator scopes: voice / lane / patch; macros modulatable | §9.2 |
| 25 | Modules expose internal signals (pitch, gate, audio out…) as routing sources | §9.3 |
| 26 | Audio-rate modulation via hybrid allowlist (PM, lin-FM, exp-FM, AM, filter cutoff, delay time to start) | §9.4 |
| 27 | **Oscillators expose four distinct destination ports: PM, linear FM, exponential FM, AM (Phase Plant-style)** | §9.5 |
| 28 | No FM-topology enum; Surge fm_routing maps to routing edges into PM ports | §9.6, §12.5 |
| 29 | LFO commitments: trigger modes, BPM sync, loop modes (normal/ping-pong/reverse/one-shot), free phase | §9.7 |
| 30 | Per-parameter effective-value + modulation-range cache with dirty flags | §9.9, §14.2 |
| 31 | **Routing-edge shaping: every routing carries an optional shape function (linear/exp/S/curve/step/invert); subsumes custom pitch-bend curves** | §9.10 |
| 32 | Namespaced stable module IDs (`surge.*`, `splurge.*`) | §10.2 |
| 33 | Additive-only evolution rules; frozen translation table; fork-don't-mutate; per-module streaming versions | §10.3–§10.4 |
| 34 | Primitives and packaged effects both ship (waveshaper + distortion + ring-mod + stereo-width + mixer + soft-clip + noise) | §10.5 |
| 35 | Multieffects dissolved; unified taxonomy; inline source crediting (e.g., "ZLowpass [AW]"); Airwindows hoisted | §10.6 |
| 36 | **Surge globals reshaped per-unit: Character per-oscillator; filter feedback as routing; per-source mute/solo on oscillator/ring/noise modules; etc.** | §10.7 |
| 37 | **Patch format: JSON document + content-hashed resource sidecar/bundle; programmatically authorable (Principle 11)** | §11.1 |
| 38 | Splurge revisions start above Surge's (e.g., 100); one-way import only | §11.1 |
| 39 | Generic content-hashed resource facility | §11.2 |
| 40 | Tags first-class; unified factory tree by category; inline author credit | §11.3–§11.4 |
| 41 | Import: perceptual equivalence; importer owns all Surge-specific knowledge; staged build with corpus regression | §12 |
| 42 | **Importer corrections from audit: 8 filter configs (not 6) differing by feedback topology not waveshaper position; fm_2and3to1 maps to both 2→1 and 3→1; full mixer channel preservation (ring-mod, noise, per-source routing/mute/solo); per-parameter flag preservation (temposync, extend_range, absolute, deform_type, deactivated, portamento sub-options); Character fanned out per oscillator** | §12.3–§12.7 |
| 43 | Frozen Surge-source-enum → Splurge-ID translation table | §12.8 |
| 44 | Always-on composite color-coded modulation ranges + value cursors; modulation-aware displays; framerate-safe | §14.1–§14.2 |
| 45 | Knob-first UI | §14.3 |
| 46 | Accessibility, undo/redo, automation recording designed in from the start | §14.8–§14.9 |
| 47 | v1 architectural foresight: generic resources, no per-voice state cap, multi-output routing, typed buses (3 voice modes × N channel layouts), FM destination ports, routing-edge shaping, feedback routing, voice-lineage data | §13 |
| 48 | Voice collapse is configured at **mix points** (phase divider, per-connection lane inputs), not on sources or lanes; sources determine what exists, mix points determine what survives | §7.4 |
| 49 | Channel layout is source-side; lane source taxonomy: generators / external inputs / lane inputs | §5.1 |
| 50 | Unison module = voice-context multiplier (stackable, mid-chain capable); lifted oscillators keep internal unison as the cheap path and the import target | §7.5 |
| 51 | Voice lineage (note → unison group → clone) + per-clone perturbations are first-class voice-context data in the v1 voice runtime | §7.5, §13 |
| 52 | Channel-splitter / M-S utility for dual-mono and mid/side chains | §10.5 |
| 53 | Preset compatibility contract: forward-loading guaranteed forever; backward explicitly not promised; clean refusal on newer revisions | §11.6 |
| 54 | Schema-evolution rules: ID-addressed self-describing objects, per-object version stamps, additive-only, explicit defaults, extension points, string enums, eternal migrations + golden-preset corpus | §11.6 |
| 55 | "Human-readable" clarified: not-a-binary-blob; schema complexity is acceptable in service of flexibility | §11.1 |
| 56 | Sound-design trick catalogue as living design-validation suite; processing-order law (forward edges same-block, backward edges 1-block) | Appendix B |
| 57 | Resample-to-wavetable, audio-input modulators (envelope follower), cross-module sync, and stutter module on the feature wishlist | §13 |

---

## Appendix A: Surge XT Codebase Reference

Orientation points for downstream planners, from the design-phase code review and a follow-up verification audit. Paths relative to the Surge XT repository root.

**Already-extracted DSP libraries (Tier A — consume as-is):**
- `libs/sst/sst-filters` (GPL-3) — the per-voice quad-SIMD filter library, halfband resampler, biquads. Consumed in Surge via `QuadFilterChain.h` and `WaveShaperEffect`.
- `libs/sst/sst-waveshapers` (GPL-3) — 40+ waveshaper transfer functions; `WaveshaperType` enum.
- `libs/sst/sst-effects` (GPL-3) — 11 effects already ported, consumed via the `SurgeFXConfig` template adapter at `src/common/dsp/effects/SurgeSSTFXAdapter.h`. Pattern: template `Config` providing `GlobalStorage`, `EffectStorage`, `ValueStorage` types. **This is the canonical adapter pattern Splurge's engine context replicates** (§3.3).
- `libs/sst/sst-basic-blocks` (GPL-3) — DSP primitives, `SincTableProvider`, lipol-like lag, clippers, resamplers, parameter metadata helpers, quadrature oscillators. **No modulation matrix here** — Surge's modulation system is in `src/common/dsp/modulators/` and is Tier B.
- `libs/eurorack/eurorack` (MIT — Surge's MI fork) — Plaits (Twist oscillator), Clouds (Nimbus effect).
- `libs/airwindows` (MIT) — vendored, used via the Airwindows wrapper.
- `libs/tuning-library` (GPL-3) — Scala/KBM file support.
- `libs/luajitlib/LuaJIT` (MIT) — Formula modulator script host.
- `libs/simde` (MIT) — SSE→NEON portability. Headers already present; not yet wired across all native intrinsics in Surge.

**Tier B lift surface (un-extracted Surge code Splurge vendors):**
- Oscillator base & factory: `src/common/dsp/oscillators/OscillatorBase.h`; `spawn_osc` in `Oscillator.cpp`; 12 types in `SurgeStorage.h:283-300`. Audio-rate FM input via `assign_fm` (raw `float*` to the modulator's `output[BLOCK_SIZE_OS]`). Internal unison lives inside the oscillators (`AbstractBlitOscillator` and peers carry per-sub-voice state — detune, drift LFOs, pan, up to `MAX_UNISON` — summed internally to one output buffer); this is the basis for keeping oscillator-internal unison as the cheap path and import target (§7.5).
- Modulation classes: `src/common/dsp/modulators/` — `ModulationSource`, `LFOModulationSource`, `ADSRModulationSource`, MSEG and Formula evaluators, step sequencer, controller sources.
- Lipol smoothers: `src/common/dsp/vembertech/lipol.h` (~1000 lines, SIMD parameter smoothing).
- Wavetable: `src/common/Wavetable.{h,cpp}` — mipmapped, shared across voices.
- Parameter class: `src/common/Parameter.h:23-31, 615` — designed for null-storage (line 615: `SurgeStorage *storage = nullptr;` "this pointer will be null for legitimate uses").
- Non-adapted effects (those not in `sst-effects`): Distortion, Vocoder, Convolution, Mid-Side Tool, EQ 3-Band, Graphic EQ 11-Band, Ring Modulator, Frequency Shifter, Resonator, Conditioner, Chorus, Tape, Combulator, CHOW, Neuron, Exciter, BBD Ensemble, Audio Input, and the Airwindows wrapper at `src/common/dsp/effects/airwindows/AirWindowsEffect.{h,cpp}` (parameter 0 selects from ~70+ algorithms).

**Engine-context surface (services Surge oscillators read from `SurgeStorage*` — Splurge engine must provide):**
- Pitch tables: `note_to_pitch()` family — referenced via `OscillatorBase.h:59-71`.
- Sample-rate state: `storage->samplerate`, `samplerate_inv`, `dsamplerate_os_inv` — instance members, not globals.
- Sinc tables: `storage->sinctable`, `sinctableI16` — used in ClassicOscillator.cpp:509-534, WavetableOscillator:441-461, WindowOscillator:361-368, and others.
- RNG: `storage->rand_01()`, `rand_u32()` — used by ClassicOscillator.cpp:238, SineOscillator.cpp:122, WavetableOscillator.cpp:136, AliasOscillator.cpp:97.
- Memory pools: `storage->memoryPools->stringDelayLines` — String oscillator only.
- Audio input buffers: `storage->audio_in[][]` — Audio Input oscillator; Alias `aow_audiobuffer` mode.
- Tempo: `storage->temposyncratio`, `temposyncratio_inv`.

**Dependency surprises confirmed by audit (Splurge must address):**
- **Character is global in Surge but reaches into individual oscillators:** `ClassicOscillator.cpp:187`, `SineOscillator.cpp:144`, `StringOscillator.cpp:378` all do `charFilt.init(storage->getPatch().character.val.i)`. Splurge makes Character per-oscillator (§10.7); importer fans out the value.
- **Alias oscillator's modes reach into arbitrary patch memory** (`AliasOscillator.cpp:154-169` accesses `scenedata`, `dawExtraState`, `stepsequences` as wavetable content). Per Principle 9: these modes are dropped (§3.3); audio-input mode is an open question (§17).
- **Static shaped_sinetable init guard** (`AliasOscillator.cpp:55-86`) — non-thread-safe lazy init; Splurge moves to engine-controlled init.
- **`param_id_in_scene` scene-array indexing:** oscillators cache scene-global parameter array indices at init time and read from `localcopy[id_shape].f` at audio rate (e.g., `ClassicOscillator.cpp:196-201, 582`). Engine must reproduce a per-lane `pdata*` array or rebase indices per-module (open question §17).

**Hardcoded counts (the constraints Splurge removes)** — `src/common/SurgeStorage.h:72-83, 157`: `n_oscs = 3`, `n_egs = 2`, `n_lfos_voice = 6`, `n_lfos_scene = 6`, `n_lfos = 12`, `n_filterunits_per_scene = 2`, `n_fx_params = 12`, `n_fx_slots = 16`, `n_send_slots = 4`, `n_scenes = 2`; `n_customcontrollers = 8` (`ModulationSource.h:130`); `MAX_VOICES = 64`, `BLOCK_SIZE = 32` with 2× oscillator oversampling (`globals.h`).

**Orchestration & voice (replaced by Splurge engine):**
- Per-block entry: `SurgeSynthesizer::process()` — `src/common/SurgeSynthesizer.cpp:4557`. Flow: input upsampling → per-scene voice rendering → quad filter batches → downsampling → scene insert FX → scene summing → send FX → global FX → master gain/clip.
- Per-voice processing: `src/common/dsp/SurgeVoice.cpp:1032` (`process_block`); oscillators placement-new'd into fixed buffers at `525-544`; modulation applied additively into `localcopy` at `1264-1283`.
- SIMD voice batching: `src/common/dsp/QuadFilterChain.h`; voices processed in 4-wide batches at `SurgeSynthesizer.cpp:4775`.

**Importer ground-truth references (used by §12):**
- Filter configs (8 values): `SurgeStorage.h:503-519` (`filter_config` enum). Topology details: `QuadFilterChain.cpp:69-387`. **Differ by feedback topology, not waveshaper position** (a correction from the earlier draft of §12.4).
- FM routing (4 values): `SurgeStorage.h:521-536` (`fm_routing` enum). The fourth mode is `fm_2and3to1`, meaning **both** OSC 2 and OSC 3 modulate OSC 1, **not** "3→1". Implementation is phase modulation with cubic depth scaling (`FM2Oscillator.cpp:107-122`).
- Scene modes (4 values): `SurgeStorage.h:162-177` (`scene_mode` enum) — single, split (with `splitkey`), dual, channel-split.
- Polyphony modes (6 values): `SurgeStorage.h:179-197` (`play_mode` enum) — poly, mono, mono single-trigger, mono fingered-portamento, mono single-trigger + fingered-portamento, latch.
- Per-scene mixer channels: `SurgeStorage.h:817-834` — `level_o1/2/3`, `route_o1/2/3` (Filter 1 / Both / Filter 2), `mute_o1/2/3`, `solo_o1/2/3`, `level_ring_12`, `level_ring_23` and their route/mute/solo siblings, `level_noise`, `noise_colour`, `route_noise`, `mute_noise`, `solo_noise`, `vca_level`, `level_pfg`.
- Per-scene signal-flow params: `SurgeStorage.h:840-841` — `filter_balance`, `feedback` (with extend_range), `filterblock_configuration`, `lowcut` (deactivatable), waveshaper unit (`wsunit.type`, `wsunit.drive`).
- Per-parameter flags: `Parameter.h:537-540` — `temposync`, `extend_range`, `absolute`, `deactivated`, `deform_type`, portamento sub-options.
- Character: `SurgeStorage.h:268-280` (cm_warm / cm_neutral / cm_bright); patch-level at `SurgePatch.h:1298`.
- Modulation: source enum at `ModulationSource.h:40-84`; routing struct at `253-266`; three lists (voice, scene, global) in `SurgeStorage.h`; MSEG ≤128 segments, Formula ≤8 outputs (`max_formula_outputs`).
- Parameter system: `Parameter.h` — `modulateable` flag, normalized-value conversion; **no existing "total modulation range" query** (the gap §9.9 fills).

**Patch system:**
- Format: FXP wrapper (`sub3` header) around XML plus embedded wavetable blobs; `SurgePatch.cpp:1126` (load), `3764` (XML save). Streaming revision `ff_revision = 28` (`SurgeStorage.h:151`); per-module streaming-mismatch handlers throughout.
- Metadata: name, category, author, license, comment, tags (`SurgePatch.cpp:3789-3798`).
- Categories derive from folder paths (factory by sound category; third-party by author folder — the layout §11.4 replaces); the in-file category attribute is ignored on preset load. SQLite-backed patch DB at `src/common/PatchDB.*`.

**GUI (replaced, findings shaped §14):**
- Editor: `src/surge-xt/gui/SurgeGUIEditor.{h,cpp}` (~8k lines combined) + ~50 supporting files; 60 Hz idle timer; modulation refresh iterates all parameters on a single flag.
- Controls: `ModulatableSlider` — knob vs. slider is orientation in skin XML, not a widget type; modulation display state is three-valued (unmodulated / modulated-by-active / modulated-by-other) with positive/negative colors only — no composite or per-source rendering.
- Oscillator preview (`OscillatorWaveformDisplay`) renders an unmodulated oscillator instance.
- Skin engine: XML-driven `.surge-skin` packages under `resources/data/skins/`.

---

## Appendix B: Sound-Design Trick Catalogue (Design Validation)

A living catalogue of nontrivial sound-design techniques — gathered from the field (Serum 2, Phase Plant, producer tutorials) — mapped onto Splurge's primitives. Each entry either validates that the architecture *composes* (the trick is a recipe built from committed primitives) or honestly names the gap it exposes. Planners treat these as candidate regression patches (§15); new tricks get added here and checked the same way. The bar throughout: **tricks should be compositions, not special-case features.**

**A general law worth stating once:** within a lane, modules process in chain order. An audio-rate routing edge pointing *forward* (an earlier module's output into a later module's port — e.g., OSC 2 into OSC 1's PM port, where the modulator is processed first) is **same-block and sample-accurate**. An edge pointing *backward* (a later module's output into an earlier module's port) is a **feedback edge** (§6.6) and carries a one-block delay by construction. Several entries below turn on this distinction.

### B.1 Amplitude-gating one source by another's waveform ✅

*Goal:* a noise generator audible only near the positive peaks of a sub oscillator's sine.
*Recipe:* sub audio out (§9.3) → noise generator's **AM port** (§9.5), with a steep custom-curve shape on the edge (§9.10) so gain opens only above ~0.8 of the sine's swing.
*Validates:* shaping living on the edge rather than the modulator; AM as a destination port.

### B.2 Pseudo-phase-distortion via inverted stereo delay-time modulation (Au5 / Serum-style) ✅

*Goal:* modulate a short stereo delay's L/R times with noise, with R receiving the inverted modulation.
*Recipe:* noise output → delay **L-time** edge (+); the same source → delay **R-time** edge with inversion shaping (§9.10). The lifted delay has independent L/R time parameters; delay time is on the audio-rate allowlist (§9.4).
*Validates:* one source feeding many edges with per-edge polarity; per-channel parameters; allowlist growability.

### B.3 Tempo-synced reece beating (Serum 2 resampling trick) ◐

*Goal:* convert free-running detune beating into tempo-locked beating — resample two phase-opposed saws into a wavetable, then sweep WT position with a tempo-synced LFO.
*Playback half:* WT position ← tempo-synced LFO — trivial (§9.7).
*Capture half:* requires **resample-to-wavetable** — on the feature wishlist (§13); architecture-ready (headless render + §11.2 resources).
*Native alternatives available without it:* (a) two identical saws, one polarity-inverted at the mixer, with one oscillator's **phase offset** swept by a tempo-synced LFO — the phase sweep *is* the beating, tempo-locked at the source; (b) generate the frames procedurally with the lifted Lua wavetable scripting.
*Validates:* phase offset as a modulatable parameter; mixer polarity; exposed the resampling gap.

### B.4 Sidechain pumping / auto-wah from audio ◐

*Goal:* duck lane B's volume (or sweep a filter) following the amplitude envelope of lane A's audio (kick-pump, auto-wah).
*Gap:* requires an **envelope follower** — a modulator that *consumes* audio and emits a control signal. Surge has no standalone envelope-follower modulator (followers exist only inside certain effects). The routing system already supports audio-rate edges, so the addition is: modulators may declare **audio-input ports** (§13). Once it exists: lane A output → follower input; follower output → lane B gain with inversion shaping.
*Workaround today:* a tempo-synced MSEG shaped like the kick's envelope (the classic "ghost sidechain"), which needs no audio input at all.
*Exposes:* the audio-input-modulator port concept — reserved in the v1 modulator API, module ships later.

### B.5 Vibrato that develops over time ✅ (with a design note)

*Goal:* LFO → pitch, but the LFO's *influence* grows as a note sustains.
*Recipe:* the LFO's **output-level parameter** is itself a modulatable module parameter — route an envelope to it. No new machinery.
*Design note:* the fully general form ("modulate any edge's depth") is an open question (§17 #17). The output-level route covers the common cases without exploding the destination space.

### B.6 Hard sync (classic sync lead) ◐

*Goal:* oscillator A's phase resets every time oscillator B completes a cycle; sweeping B's pitch gives the classic ripping sync sound.
*Supported today:* the lifted Classic/Modern oscillators carry **internal sync** parameters — the classic case works per-oscillator, as in Surge.
*Gap:* arbitrary *cross-module* sync (any oscillator hard-synced to any other) needs an **event-typed signal** — a phase-reset destination port, distinct from audio and control signals. Oscillators already emit gate/retrigger as sources (§9.3); the missing half is the typed destination and the routing rule (§17 #18).
*Exposes:* whether v1's bus model reserves an event signal type.

### B.7 Self-FM / operator feedback ✅ (with the block-delay law)

*Goal:* an oscillator modulating its own phase (the FM-operator feedback sound).
*Supported today:* the lifted Sine oscillator has **per-sample internal feedback** as a parameter — the canonical case, fully supported.
*The law applies:* wiring an oscillator's output back into its own PM port via the general routing system is a *backward* edge → one-block delay → audibly different (and mushier) than per-sample feedback. Per-sample self-feedback stays a module-internal feature; the general routing system does not pretend to provide it.

### B.8 Live granular mangling of external input ✅

*Goal:* granular texture from a live input (effects-mode hosting).
*Recipe:* external input lane (§5.1) → **Nimbus** (lifted; it is literally Mutable Clouds) in the lane's chain. Live granular as an effect, today. The future granular *oscillator* (§13) covers the sample-resource case; Nimbus covers the live case.

### B.9 Karplus-Strong plucks from first principles ✅ / ◐

*Supported today:* the lifted **String oscillator** *is* Karplus-Strong (noise burst into tuned feedback delay lines) — the sound is one module away.
*The general construction* (build it yourself: noise generator → short delay with feedback edge (§6.6) → keytrack-scaled delay time for tuning) additionally needs a **per-voice-context delay module** — nothing in the architecture forbids one (context capability, §6.3), but the lifted delay is post-mix. An additive module, listed for the library backlog.
*Validates:* feedback edges + keytrack-shaped routing; exposes the per-voice delay gap.

### B.10 Audio-rate panning (rotary sidebands) ◐

*Goal:* pan position modulated at audio rate — beyond ~20 Hz, panning becomes timbre (sidebands), a classic spatial-distortion trick.
*Today:* control-rate autopan (LFO → pan) is trivial. Audio-rate pan requires adding **pan** to the §9.4 allowlist plus module-side per-sample pan support — exactly the growth path the allowlist was designed for; no architectural change.

### B.11 Mutual per-sample cross-modulation between modules ❌ (by design)

*Goal:* filter A's output modulates filter B's cutoff *while* B's output modulates A's cutoff, per-sample (zero-delay-feedback modulation networks).
*Verdict:* **not supported, deliberately.** One direction of any modulation loop is necessarily a backward edge → one-block delay (§6.6). True ZDF modulation *networks* across arbitrary modules require solving implicit equations per sample across the graph — full Reaktor Core territory, explicitly outside Splurge's semi-modular boundary (§1.3). Module-*internal* ZDF (inside a single filter) is unaffected.

### B.12 Pitch-tracking external audio ❌ (for now)

*Goal:* detect the pitch of an external input and drive oscillators with it (audio-to-synth).
*Verdict:* not in the current design. Would be a **pitch-detector** audio-input modulator (same port concept as B.4) plus genuinely hard, latency-prone DSP. Honest wishlist material; nothing in the architecture blocks it, but nothing supports it yet either.

### B.13 Stutter / buffer-repeat ◐

*Goal:* tempo-synced capture-and-repeat glitching.
*Verdict:* no current module does this; it is a straightforward **additive module** (a buffer-capture effect with tempo-synced retrigger), no architectural change. Listed in §13.

### B.14 How to use this appendix

When a new trick is proposed: decompose it into routing edges, ports, shaping functions, bus/collapse operations, and modules. If it decomposes — add the recipe here and a patch to the test corpus. If it doesn't — name the missing primitive precisely and take it to §13 (feature) or §17 (design question). Entries marked ✅ are pure compositions; ◐ entries work partially or with a named addition; ❌ entries are out of scope with reasons stated.
