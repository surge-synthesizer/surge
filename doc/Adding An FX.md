# How to add an effect

This tutorial will demonstrate how to add an example effect called `MyEffect`.

1. Create `src/common/dsp/effects/MyEffect.h` and `MyEffect.cpp`. Look at Flanger as an example.
   * The parameters for your effect should be placed in a named enum in header.
   * Override `init_ctrltypes()` to initialize the parameter names and types,
     and `init_default_values()` to set the default parameter values.
   * The guts of your signal processing should go in the `process()` function.

2. In `src/common/dsp/Effect.cpp` add the newly created header file to the list of includes at the top.
   Then in `spawn_effect()` add a case to spawn your new effect:

```cpp
case fxt_myeffect:
    return new MyEffect(storage, fxdata, pd);
```

Note that we have chosen the effect ID `fxt_myeffect` for our example effect.

3. In SurgeStorage.h:
* Add an enum to `fx_type` with your effect ID.
* Add the name of your effect to `fx_type_names`.
* Add the shorter name of your effect to `fx_type_shortnames`. This is used in Surge XT effect type menu itself (right side),
  so make sure it fits there!
* Add a 3 or 4 character acronym of your effect to `fx_type_acronyms`.
  Verify if this fits properly in the FX grid boxes on the GUI!

4. In `resources/surge-shared/configuration.xml` add your effect to the `fx` XML group.
   Later, you may also add snapshots for presets of your effect. 
   `"init"` goes here, create any further factory presets in `resources/data/fx_presets`   

5. In `src/common/CMakeLists.txt`, add the path to your .cpp and .h under `add_library`,
   search the file for `Chorus` for example, to see where it goes.
   Please place your effect in alphabetical order here.

Note that the configuration file DOES NOW automatically reload when you compile and run the plugin!
(A full build may be required though)

Now you can finish coding up your effect!