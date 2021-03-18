# How to add a effect

This tutorial will demonstrate how to add an example effect called `MyEffect`.

1. Create `src/common/dsp/effect/MyEffect.h` and `MyEffect.cpp`. Look at Flanger as an example.
   * The parameters for your effect should be placed in a named enum in header.
   * Override `init_ctrltypes()` to initialize the parameter names and types,
     and `init_default_values()` to set the default parameter values.
   * The guts of your signal processing should go in the `process()` function.

2. In `Effect.cpp` add the newly created header file to the list of includes at the top.
   Then in `spawn_effect()` add a case to spawn your new effect:

```cpp
case fxt_myeffect:
    return new MyEffect(storage, fxdata, pd);
```

Note that we have chosen the effect ID `fxt_myeffect` for our example effect.

3. In SurgeStorage.h:
* Add an enum to `fx_type` with your effect ID.
* Add the name of your effect to `fx_type_names`.
* Add a 3 or 4 character shorthand name of your effect to `fx_type_shortnames`.
  Verify if this fits properly in the FX grid boxes on the GUI!

4. In `resources/data/configuration.xml` add your effect to the `fx` XML group.
   Later, you may also add snapshots for presets of your effect.

5. In `CMakeLists.txt`, add path to your .cpp under `SURGE_SHARED_SOURCES`,
   search the file for `Chorus` for example, to see where it goes.
   Please place your effect in alphabetical order here.

Note that the configuration file DOES NOT automatically reload when you compile and run the plugin!
Whenever you make changes to this file, you must copy it to the correct "installed" location where
the plugin is expecting to find it. You can find this file by opening the factory data folder
(`Menu > Data Folders > Open Factory Data Folder...`).

Then you can finish coding up your effect!
