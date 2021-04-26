# Surge Architecture

This document is a guide for Surge developers to understand how the program hangs together
and works. This is not a developer how-to. That information is covered in [our Developer Guide](./Developer%20Guide.md).

**This document is still being written. As we fix things, if we remember, we add them to this
document for our future selves**. As such, it is rather casual and incomplete now. Sorry!

* [Code Layout](#code-layout)
* [SurgeStorage, Parameter and SurgeSynthesizer](#parameter-and-surge-synthesizer)
* [SurgeGUIEditor](#surgegui)
* [Voices and MPE](#voices-and-mpe)
* [DSP](#dsp)
* [Headless](#headless)

# Code Layout

Third-party libraries are copied or submoduled into `libs`. All of the Surge C++ source code is in the `src` directory.

`src/common` contains the complete engine code of Surge, all the DSP and voice handling logic, and so on.
`src\gui` contains the user interface code, along with custom UI widget classes, various helpers, etc.

Each of the plugin flavors is handled by JUCE plugin wrappers, which can be found in `src/surge_synth_juce`.
Additionally, we also have a headless flavor in `src/headless`.

# SurgeStorage, Parameter and SurgeSynthesizer

Code collaboration between various parts of Surge happens through the
SurgeStorage class, which acts as a named bag of parameters. Each parameter
is represented by the Parameter class, which holds type and value and has
an update semantic. SurgeSynthesizer reads the values from the current SurgeStorage at runtime.

This means that things like loading a new patch amount to unstreaming SurgeStorage
into SurgeSynthesizer's current Storage instance. Similarly, changing a value in the UI
means locating the appropriate set of Parameters and calling the appropriate invalidating
value change function on them.

# SurgeGUIEditor

SurgeGUIEditor collaborates with the Parameter set as outlined above.

Critically, it also has an ::idle thread which allows parameter changes from outside
of the UI (like those delivered by the host) to be reflected in the UI.

# Voices and MPE

Surge voice allocation and voice stealing code is complicated, supporting multiple polyphony modes
and MPE, which adds substantial complexity. The core data structure is a list of voices
which are allocated at startup and configured with an in-place contructor. Each has a reference
to state for the channel in which it is playing.

The voice class is in `src/common/dsp/SurgeVoice.h` and contains a `SurgeVoiceState`.

# DSP

The most important thing about the DSP engine is that it contains hand-coded SSE2 implementations of
most activities. This makes the engine fast, but also requires all data structures to align
on 16 byte boundaries.

# Headless

The headless host makes an executable of Surge without a UI attached. Right now, it is primarily used for debugging and testing.
