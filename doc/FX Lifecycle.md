# FX Lifecycle

This documents how FX are created, swapped, initialized, etc... as of
8e03ba0b54b09 or so. 

There's lots of cases. Lets enumerate all of them

## Core data structures and Functions

* `spawn_effect` creates an instance of an Effect with the appropriate type bound to a storage and data
* `loadFx` compares the synth state with the runtime state and optionally reloads effects
    * the first argument `initp` if true means the new effect calls `init_default_values` and if false
      just forces all params within their default ranges
    * the second `force_reload_all` makes all fx reload
    * loadFX begins in all cases where a type switches setting all params to type 'none'.
    * `loadFx` is called from three places
        * synth::processThreadunsafeOperations if `load_fx_needed` is true with false/false
        * synth::processControl if `load_fx_needed` with false/false
        * synth::loadRaw in all cases with false/true (namely, force a full reload on patch load)
    * If a reload of either form has changed, the `updateAfterReload()` virtual method is called on the FX
* `load_fx_needed` is a bool on SurgeSynth
    * triggers a call to loadFx in processControlEV
    * set to true in the constructor of synth
    * set to true on a setParameter01 of ct_fxtype (so automation path)
    * set to false in loadFx
    * set to true in SGE when you have a tag_fx_menu
* The patch has the `fx[n_fx]` array which holds the FXStorage pointers
* The synth has an `FXStorage *fxsync` which is malloced to `n_fx_slots * sizeof(FXStorage)`. This
  object acts as an edit buffer for external editors who want to bulk update the type of objects.
  * In the synth constructor it is set to the value of `patch.fx[i]` for each i
  * In SurgeGUIEditor the fxsync is used as the storage which is edited by the CSnapshotMenu, not
    the patch buffer
  * In `loadFX`
     * if reloading an FX, copy from the sync to the patch. This defacto means "take the sync values as
        they have been set by a UI"
     * Similarly if fx_reload[s] is true, copy from the sync and syspend and init the FX
* The slot `fx_reload[n_fx_slots]`
  * This is set by SurgeGUIEditor primarily to force a reload of an FX in the event that the type has not changed

## Empty FX to Loaded FX, FX On

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

* CSnapshotMenu::loadSnapshot
    * Load an FX and call `init_ctrltypes` and `init_default_values`. This sets up the params on the control to defaults
      in the storage bound to the FX
    * delete the FX
    * Parse the XML and set the fxbuffer parameter values as described in the FX. This leaves the storage configured with
       values and types but does not leave the synth with the FX setup
    * raise a valueChanged so SurgeGuiEditor sets load_fx_needed to true. 
    * If audio_processing_active is false it also sets up the FX from the UI thread via the
      call to synth->processThreadunsafeOperations() but we will ignroe that case in this document
      from hereon out
* SurgeSynthesizer
    * In the next `process()` and call to `processControl` `load_fx_needed` will be true
    * so call `loadFx(false,false)` which will force FX where the type in the FX to be noticed as
      changed and will spawn an fx
    * but since `initp` is false, don't call `init_default_values` and instead continue to use the values
      left lingering in the FX from the invocation in CSnapshotMenu.
    
## Loaded FX to Off, FX On

* CSnapshotMenu calls loadSnapshot with type 0 and null XML (fine).
* This sets the valtype to 0 and then SGE gets the message
* Question: What sets the types to none everywhere?

## Empty FX to Loaded FX, FX Bypassed

## Active FX to New preset of Same Type, FX On

* CSnapshotMenu 

## Active FX to New FX of Different Type, FX On

## Active FX to New FX, FX Bypassed

Question: is bypass from CEG different than global bypass?

## Load a patch with FX Off

## Load a patch with active FX

## Load a User FX setting

* CFXMenu::loadUserPreset looks like loadSnapshot in that it creates a new FX on the sync buffer, applies the value
  from the preset, and raises the event
* After that it's just like a same-type load

## Swap an FX

## An Airwindows FX is activated and changes type

## An Airwindows FX is de-activated and changes type

## A non-Airwindows FX becomes an Airwindows FX in activated state

## A non-Airwindow FX becomes an Airwindows FX in deactivated state

## The DAW Automation Path

The DAW automation path comes in through `synth::setParameter01` and handles the `ct_fxtype` to do the spawn-and-replace.
See the comments there for more.