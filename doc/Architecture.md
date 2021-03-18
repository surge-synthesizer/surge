# Surge Architecture

This document is a guide for Surge developers to understand how the program hangs together
and works. This is not a developer how-to. That information is covered in [our Developer Guide](./Developer%20Guide.md).

**This document is still being written. As we fix things, if we remember, we add them to this
document for our future selves**. As such, it is rather casual and incomplete now. Sorry!

**Table of Contents**

* [Code Layout](#code-layout)
* [SurgeStorage, Parameter and SurgeSynthesizer](#parameter-and-surge-synthesizer)
* [SurgeGUIEditor](#surgegui)
* [Voices and MPE](#voices-and-mpe)
* [DSP](#dsp)
* [Hosts](#hosts)
* [VSTGUI](#vstgui)

# Code Layout

Third-party libraries are copied or submoduled into `libs`. VST3SDK and VSTGUI are an exception. They
are submodules in the top level directory. All of the Surge C++ source is in the `src` directory.

`src/common` contains common code; `src/common/gui` and `src/common/dsp` contain the GUI and DSP code
respectively.

Each of the hosts has a directory in `src`: `src/vst2`, `src/vst3`, `src/au` and `src/headless` are the
four flavors of hosts we have now.

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
the UI (like those delivered by the host) to be reflected in the UI.

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

# Hosts

The three traditional hosts (VST2, 3 and AU) are pretty straightforward.

The headless host makes an executable of Surge without a UI attached. Right now, it is
primarily used for debugging and testing.

# VSTGUI

See (our documentation on how we use VSTGUI in Surge)[vstgui-dev.md].