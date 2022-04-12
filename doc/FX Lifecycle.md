# Effects Lifecycle

This documents how effects are created, swapped, initialized, etc... as of commit `8e03ba0b54b09` or so. 

There's lots of cases. Let's enumerate all of them:

## Core data structures and Functions

* `spawn_effect` creates an instance of an `Effect` with the appropriate type bound to a storage and data
* `loadFx` compares the synth state with the runtime state and optionally reloads effects
    * the first argument `initp` if true means the new effect calls `init_default_values` and if false
      just forces all params within their default ranges
    * the second `force_reload_all` makes all effects reload
    * `loadFx` begins in all cases where a type switches setting all params to type 'none'.
    * `loadFx` is called from three places
        * `synth::processAudioThreadOpsWhenAudioEngineUnavailable` if `load_fx_needed` is true with false/false
        * `synth::processControl` if `load_fx_needed` with false/false
        * `synth::loadRaw` in all cases with false/true (namely, force a full reload on patch load)
    * If a reload of either form has changed, the `updateAfterReload()` virtual method is called on the effect
* `load_fx_needed` is a bool on `SurgeSynth`
    * triggers a call to `loadFx` in `processControlEV`
    * set to true in the constructor of synth
    * set to true on a `setParameter01` of `ct_fxtype` (so automation path)
    * set to false in `loadFx`
    * set to true in `SurgeGUIEeditor` when you have a `tag_fx_menu`
* The patch has the `fx[n_fx]` array which holds the `FXStorage` pointers
* The synth has an `FXStorage *fxsync` which is malloced to `n_fx_slots * sizeof(FXStorage)`. This
  object acts as an edit buffer for external editors which want to bulk update the type of objects.
  * In the synth constructor it is set to the value of `patch.fx[i]` for each `i`
  * In `SurgeGUIEditor` the `fxsync` is used as the storage which is edited by the `XMLConfiguredMenus`, not
    the patch buffer
  * In `loadFx`
     * if reloading an effect, copy from the sync to the patch. This de facto means "take the sync values as
        they have been set by the GUI"
     * Similarly if `fx_reload[s]` is true, copy from the sync, thensuspend and initialize the effect
* The slot `fx_reload[n_fx_slots]`
  * This is set by `SurgeGUIEditor` primarily to force a reload of an effect in the event that the type has not changed

## Empty effect to loaded effect, effect enabled

Spawn Effect is called twice

```
Spawn Effect  id=6
-------- StackTrace (7 frames of 40 depth showing) --------
  [  1]: 1   Surge                               0x000000015000dc72 _Z12spawn_effectiP12SurgeStorageP9FxStorageP5pdata + 130
  [  2]: 2   Surge                               0x0000000150214423 _ZN7CFxMenu12loadSnapshotEiP12TiXmlElementi + 227
  [  3]: 3   Surge                               0x000000015022bc4e _ZZN13CSnapshotMenu30populateSubmenuFromTypeElementEP12TiXmlElementPN6VSTGUI11COptionMenuERiS5_RKlS5_ENK3$_0clEPNS2_16CCommandMenuItemE + 62
  [  4]: 4   Surge                               0x000000015022bbf2 _ZNSt3__1L8__invokeIRZN13CSnapshotMenu30populateSubmenuFromTypeElementEP12TiXmlElementPN6VSTGUI11COptionMenuERiS7_RKlS7_E3$_0JPNS4_16CCommandMenuItemEEEEDTclclsr3std3__1E7forwardIT_Efp_Espclsr3std3__1E7forwardIT0_Efp0_EEEOSE_DpOSF_ + 50
  [  5]: 5   Surge                               0x000000015022bb92 _ZNSt3__128__invoke_void_return_wrapperIvE6__callIJRZN13CSnapshotMenu30populateSubmenuFromTypeElementEP12TiXmlElementPN6VSTGUI11COptionMenuERiS9_RKlS9_E3$_0PNS6_16CCommandMenuItemEEEEvDpOT_ + 50
  [  6]: 6   Surge                               0x000000015022bb42 _ZNSt3__110__function12__alloc_funcIZN13CSnapshotMenu30populateSubmenuFromTypeElementEP12TiXmlElementPN6VSTGUI11COptionMenuERiS8_RKlS8_E3$_0NS_9allocatorISB_EEFvPNS5_16CCommandMenuItemEEEclEOSF_ + 50
Spawn Effect  id=6
-------- StackTrace (7 frames of 38 depth showing) --------
  [  1]: 1   Surge                               0x000000015000dc72 _Z12spawn_effectiP12SurgeStorageP9FxStorageP5pdata + 130
  [  2]: 2   Surge                               0x0000000150107b73 _ZN16SurgeSynthesizer6loadFxEbb + 1155
  [  3]: 3   Surge                               0x000000015010c5f3 _ZN16SurgeSynthesizer14processControlEv + 3667
  [  4]: 4   Surge                               0x000000015010ccb2 _ZN16SurgeSynthesizer7processEv + 1234
  [  5]: 5   Surge                               0x000000015063eb2d _ZN7aulayer6RenderERjRK14AudioTimeStampj + 1277
  [  6]: 6   Surge                               0x0000000150640620 _ZN6AUBase9RenderBusERjRK14AudioTimeStampjj + 96
```

* `XMLConfiguredMenus::loadSnapshot`
    * Load an effect and call `init_ctrltypes` and `init_default_values`. This sets up the params on the control to defaults
      in the storage bound to the effect
    * delete the effect
    * Parse the XML and set the `fxbuffer` parameter values as described in the effect. This leaves the storage configured with
       values and types but does not leave the synth with the effect setup
    * raise a `valueChanged` so `SurgeGUIEditor` sets `load_fx_needed` to true. 
    * If `audio_processing_active` is false it also sets up the effect from the UI thread via the
      call to `synth->processAudioThreadOpsWhenAudioEngineUnavailable()`, but we will ignroe that case in this document
      from hereon out
* `SurgeSynthesizer`
    * In the next `process()` and call to `processControl`, `load_fx_needed` will be true
    * so call `loadFx(false, false)` which will force effect where the type in the effect is to be noticed as
      changed and will spawn an effect
    * but since `initp` is false, don't call `init_default_values` and instead continue to use the values
      left lingering in the effect from the invocation in `XMLConfiguredMenus`.
    
## Loaded effect to disabled, effect enabled

* `XMLConfiguredMenus` calls `loadSnapshot` with type 0 and null XML (fine).
* This sets `valtype` to 0 and then `SurgeGUIEditor` gets the message
* Question: What sets the types to none everywhere?

## Empty effect to loaded effect, effect bypassed

## Active effect to new preset of same type, effect on

* `XMLConfiguredMenus`

## Active effect to new effect of different type, effect on

## Active effect to new effect, effect bypassed

Question: is bypass from CEG different than global bypass?

## Load a patch with effect off

## Load a patch with active effect

## Load a User effect setting

* `FXMenu::loadUserPreset` looks like `loadSnapshot` in that it creates a new effect on the sync buffer, applies the value
  from the preset, and raises an event
* After that it's just like a same-type load

## Swap an effect

## An Airwindows effect is activated and changes type

## An Airwindows effect is deactivated and changes type

## A non-Airwindows effect becomes an Airwindows effect in activated state

## A non-Airwindow effect becomes an Airwindows effect in deactivated state

## The DAW automation path

The DAW automation path comes in through `synth::setParameter01` and handles the `ct_fxtype` to do the spawn-and-replace.
See the comments there for more.