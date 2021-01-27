# How to add a effect

This tutorial will demonstrate how to add an example effect called `MyEffect`.

1. Create `src/common/dsp/effect/MyEffect.h` and `MyEffect.cpp`. Look at Flanger as an example.
   - The parameters for your effect should be placed in an enum.
   - Override `init_ctrltypes()` to initialise the parameter names and types, and `init_default_values()` to set the default parameter values.
   - The guts of your signal processing should go in the `process()` function.

2. In `Effect.cpp` add the newly created header file to the list of includes at the top. Then in `spawn_effect()` add a case to spawn your new effect:
```cpp
case fxt_myeffect:
    return new MyEffect( storage, fxdata, pd );
```

Note that we have chosen the effect ID `fxt_myeffect` for our example effect.

3. In SurgeStorage.h:
* add an enum to `fx_type` with your effect ID.
* add the name of your effect to `fx_type_names`.

4. In `resources/data/configuration.xml` add your effect to the `fx` XML group. Later, you may also add "snapshots" for presets of your effect.

Note that the configuration file DOES NOT automatically reload when you compile and run the plugin! Whenever you make changes to this file, you must copy it to the correct "installed" location where the plugin is expecting to find it. For Windows,this is `C:/ProgramData/Surge/configuration.xml`.

Then you can finish coding up your effect!