# Splurge: Design Critique (Open Technical Problems)

**Status:** Output of the 2026-10 design review of `Splurge Architecture.md`. Factual errors and internal inconsistencies found in that review have already been corrected in the design doc. This file holds the remaining items: design-level problems that need a decision, and judgment calls worth revisiting. Each item names the design-doc sections it affects and a recommended resolution. When an item is resolved, fold the decision into the design doc and delete it here.

Line references are to the Surge XT source in this repository.

---

## Part 1: Design problems requiring a decision

### 1. Intra-lane feedback delay is ~64× longer than Surge's (§6.6, §12.4, Appendix B law)

**Problem.** The design makes every intra-lane feedback edge one *block* late. Surge's filter-block feedback is one *sample* late: `ProcessFBQuad` loops over `BLOCK_SIZE_OS` samples and feeds `d.FBlineL`/`FBlineR` (the previous sample's output) back into the current sample's input (`src/common/dsp/QuadFilterChain.cpp:107-145`, and every config except `fc_serial1`). At 48 kHz with 2× oversampling and a 32-sample block, one block is 64 OS samples, about 0.67 ms. A resonant filter feedback loop with that much latency stops behaving as resonance and becomes a comb/flanger with a ~1.5 kHz fundamental. Seven of eight Surge filter configs use this feedback path, so §12.4's import mapping cannot achieve "perceptual audio equivalence" (§12.1) as written. Appendix B.7, B.9, and B.11 also lean on the one-block law.

**Root cause.** The per-voice phase is specified as a sequence of modules, each processing a whole block before the next runs. Under that model, intra-block loops are impossible by construction.

**Recommended resolution.** Make the per-voice phase *sample-interleaved* for voice-batchable modules. Each voice-batchable module exposes a per-sample SIMD step (process one quad of samples given one quad of inputs) in addition to, or instead of, a block entry point. The engine compiles a contiguous run of such modules into a single per-sample loop, exactly as Surge's templated `ProcessFBQuad` does today. Within such a run, a backward edge has a one-sample delay and matches Surge. The one-block rule then applies only to edges that leave the run (to a non-batchable module, across the phase divider, or across lanes). This is also cheaper to lift: sst-filters' `QuadFilterUnit` and sst-waveshapers are already per-sample function pointers. Oscillators remain block-granular sources feeding the run.

### 2. Surge's FM input is not one thing; the four-port model does not absorb it (§9.5, §12.5)

**Problem.** `assign_fm` hands the carrier a raw buffer; what the carrier does with it varies by type: phase offset (Sine `SineOscillator.cpp:938`, FM2 `:139`, FM3 `:174`, Modern `:393`, Alias `:314`), a clamped multiply of the phase increment (Classic `ClassicOscillator.cpp:640`, S&H `:403`, Wavetable `:712`, Window `:449`), a pitch offset (String `:723`), or Plaits' own FM input (Twist `:388`). Mapping every imported edge to the PM port changes the sound of every Classic/Wavetable/Window FM patch. Separately, §9.5 says PM, linear FM, exp FM, and AM "require small additions" to each lifted oscillator. For the BLIT/BLEP-based oscillators (Classic, Wavetable, Window, S&H) true phase modulation is not a small addition: their phase is event-scheduled at integer-sample boundaries, which is why Surge uses increment multiplication there.

**Recommended resolution.** Keep the lifted `assign_fm` facility as a fifth, oscillator-defined port ("Native FM" or "Surge FM") whose semantics are whatever the lifted oscillator implements. The importer targets that port exclusively, with depth = `db_to_linear(fm_depth)`. The four Phase Plant-style ports are additive and implemented per oscillator where feasible; oscillators that cannot support a port simply do not declare it (consistent with §6.3's capability-declaration pattern). Record in §9.5 which lifted oscillators are expected to support which ports.

### 3. Generator processing order vs. the forward/backward edge law (§9.5, Appendix B preamble, §12.5)

**Problem.** Appendix B states that a forward edge (earlier module → later module) is same-block and a backward edge is one-block delayed. Surge processes OSC 3, then OSC 2, then OSC 1 (`SurgeVoice.cpp:1064-1150`) so that modulators are rendered before carriers. If an imported lane lists the oscillators as 1, 2, 3 (as §12.3 says: "in order"), then `OSC 2 → OSC 1` is a *backward* edge and acquires a block of latency on every imported FM patch.

**Recommended resolution.** Generator-to-generator audio-rate edges should be scheduled by dependency, not by display order: the engine topologically sorts generators within a lane by their audio-rate edges each time routing changes, and only a true cycle among generators falls back to the one-block rule. Display order stays a UI concern. State this in §9.5 and amend the Appendix B law to say "chain order" means scheduled order.

### 4. Oversampling domains are unspecified (§4.4, §6, §7.4, §9.4, B.2, B.9)

**Problem.** In Surge the entire per-voice path (oscillators, filters, waveshaper, feedback, ring modulators) runs at 2× (`BLOCK_SIZE_OS`); decimation to base rate happens once per scene after voices are summed (`SurgeSynthesizer.cpp:5015,5034`), which is exactly the location of Splurge's phase divider (§6.2). The design doc never mentions this. Open consequences:

- Moving the divider earlier or later (§6.2 "power users may force an earlier transition") moves the decimator. Is a post-mix-only module placed early forced to run at 2×, or is a decimator inserted before it?
- Per-note and per-voice collapse (§7.4) that carries N strands into a downstream lane must either decimate each strand (N halfband filters) or run the downstream lane at 2×. Both are costs the doc does not budget.
- Audio-rate edges from a post-mix (base-rate) signal into a per-voice (2×) destination, or vice versa, need resampling. §9.4's allowlist does not say which domain each destination lives in.
- A per-voice delay module (B.9) runs at 2× and its delay-time parameter must be scaled accordingly.
- Sinc tables and `dsamplerate_os_inv` in the engine context (§3.3) are 2×-domain services; the context needs both rates.

**Recommended resolution.** Add a short "rate domains" section to §6: phase 1 is the oversampled domain, phase 2 is the base domain, the divider is the decimator, and every module declares its domain alongside its context capability. Audio-rate allowlist entries name their domain. Decide whether cross-lane strands are decimated per strand at the producing lane's divider (recommended; cost scales with strands anyway).

### 5. Audio-rate filter cutoff is a DSP project, not an allowlist entry (§9.4)

**Problem.** Surge's filters compute coefficients once per block (`SurgeVoice.cpp:1470-1480`, `FilterCoefficientMaker::MakeCoeffs`) and linearly interpolate them per sample. Audio-rate cutoff requires per-sample coefficient generation, which involves transcendental functions for most filter types and is not something sst-filters offers. The allowlist text treats cutoff as a plumbing change.

**Recommended resolution.** Either remove filter cutoff from the initial allowlist (PM/FM/AM and delay time remain), or budget a dedicated work item: per-filter-type per-sample coefficient paths, probably via lookup tables, and a per-filter capability flag so only filters that implement it accept the audio-rate edge.

### 6. Strand lifetime for per-note and per-voice collapse is undefined (§7.4, §5.4)

**Problem.** When a lane's input mix point is set to per-note or per-voice, the consuming lane instantiates one module instance per strand. A consuming lane has no note lifecycle. Unanswered:

- When does a strand die? A per-note reverb or ringing filter must outlive the upstream voice's release. Options: fixed hold after upstream voice end, tail detection by level, or a per-connection release time.
- Where does the memory come from? Module instances cannot be allocated on the audio thread. The engine needs pre-allocated per-strand instance pools sized by an explicit cap, and the cap must be visible to the user.
- How does upstream voice stealing propagate? If the upstream voice is stolen, the downstream strand should be released or re-targeted, not orphaned.
- How are strands batched? SIMD voice batching (§8.1) packs 4 voices; strand sets that change mid-block need repacking rules.

**Recommended resolution.** Define a strand lifecycle in §7.4: a strand is created when its upstream voice first produces output on the connection, is marked releasing when the upstream voice ends, and is reclaimed after a per-connection tail time or when silent for N blocks. Pools are sized `max_voices × consumers` per lane, preallocated when routing changes (off the audio thread), with a hard cap and a UI warning.

### 7. Mid-chain injection is required by the import and the lane model has no place for it (§6.5, §10.7, §12.3, §12.4)

**Problem.** In every serial config, oscillators routed to "Filter 2 only" are summed into the chain *after* Filter 1 and the waveshaper, immediately before Filter 2 (`QuadFilterChain.cpp:130`, the `y = x + d.DR[k]` step). §6.5 says there are no privileged positions and §12.3 says routes become "wires from each oscillator to either or both filter modules," but a linear module chain has only one input: the lane head. There is no stated way to add a signal into the middle of the chain.

**Recommended resolution.** Give modules an optional **mix-in (sum) input port** that any audio-rate source can target, summed with the module's chain input before processing. This is the general facility; the importer uses it for Surge's Filter 2 routes and it also covers "add a sub oscillator after the filter" without a sub-lane. Alternatively, make the Mixer utility (§10.5) placeable mid-chain with the chain signal as one of its inputs. Either way, state it in §6.5 and §10.5.

### 8. Surge's hardwired modulations are missing from the import mapping (§12.3, §12.9)

**Problem.** Several Surge behaviors are not routings in Surge and so cannot be translated by the source-translation table (§12.8). They are parameters or fixed wiring the importer must *synthesize* routings for:

- The amplitude envelope (`ms_ampeg`) is hardwired to the voice VCA.
- Velocity sensitivity is hardwired to amplitude.
- Each filter's **Env Mod** parameter (`envmod`, `SurgeStorage.h:785`) ties the filter envelope (`ms_filtereg`) to cutoff with a depth.
- Each filter's **Keytrack** parameter (`SurgeStorage.h:786`) ties note pitch to cutoff.
- Scene `pitch`, `octave`, `pan`, `width`, `volume` and the voice `keytrack_root` are scene-level values that must land on the lane or on each oscillator.

§12.3's table does not list the two envelopes at all, and §12.9 claims "all modulation routings" are preserved.

**Recommended resolution.** Add a sub-table to §12.3 listing each hardwired Surge modulation and the Splurge routing edge or lane parameter the importer synthesizes for it. Decide whether the lane's output gain has a default voice-scope envelope slot (sane default for §5.6) or whether the AEG is an ordinary voice-scoped modulator routed to lane gain.

### 9. Per-voice parameters have no single "effective value" for the UI cursor (§9.9, §14.1)

**Problem.** §9.9 defines a per-parameter effective-value cache and §14.1 promises a live cursor per control. Per-voice parameters (anything modulated by an envelope, velocity, or voice LFO) have one effective value *per voice*. The doc does not say which value the cursor shows, and Surge avoids the problem by showing depth ranges rather than a live value.

**Recommended resolution.** Define an aggregation policy: cursor shows the most recently triggered voice's value (cheap, musically intuitive), with the composite range still showing the union across voices. Note the cost: writing per-parameter range data every block for every modulated parameter is itself O(parameters) on the audio thread; use a lock-free snapshot (seqlock or double buffer) and set dirty flags only when the quantized value changes.

---

## Part 2: Judgment calls worth revisiting

### 10. Lane-grain threading buys little for the common patch (§8.2)

For a single-lane patch, which is the default experience (§5.6) and the shape of most imported Surge patches (two lanes, with voices dominating cost), lane-grain parallelism yields nothing. Surge's cost is concentrated in voice rendering. The doc rejects voice-batch-grain threading because work units are "microseconds," but a 4-voice quad batch rendering oscillators, filters, and waveshaper over 64 OS samples is tens of microseconds, and the whole block budget at 48 kHz / 32 samples is 667 µs. Per-block fork/join over voice batches with a persistent pool is a common polysynth design and is where the headroom actually is. Recommendation: keep the lane DAG, but design the voice runtime so voice batches within a lane are independently schedulable too, and decide the grain empirically with a prototype.

### 11. Lifting `OscillatorStorage`/`FxStorage` is more entangled than §3.2 implies

`OscillatorStorage`, `FxStorage`, and `SurgeSceneStorage` are declared in `src/common/SurgeStorage.h` (lines 746, 817, 851) alongside `SurgePatch` and the full orchestrator storage; `OscillatorStorage` also inherits `CountedSetUserData` for wavetable reference counting and carries a `Wavetable` member. Oscillators additionally reach `storage->getPatch()` (Character) and `oscdata->wt` / `storage->wt_list` (Wavetable oscillator). The §3.3 engine-context surface should be extended to include wavetable library access and wavetable reference-counting, and the lift plan should budget for carving these structs out of `SurgeStorage.h` rather than assuming `Parameter.h` is the only dependency.

### 12. Items to verify against populated submodules

The submodules were empty in the reviewed checkout. Two licensing statements in the design doc are from memory and should be confirmed against the actual `LICENSE` files before relying on them:

- `libs/tuning-library` is stated as MIT.
- `libs/oddsound-mts/MTS-ESP` (libMTSClient) is stated as 0BSD.

Also unverified: the Airwindows algorithm count ("~70+") in §10.1 and Appendix A.
