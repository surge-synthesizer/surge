# Surge Architecture

This document is a guide for Surge developers to understand how the program hangs together
and works. This is not a developer how-to. That information is covered in [our Developer Guide](./Developer%20Guide.md).

**This document is still being written. As we fix things if we remember we add them to this
document for our future selves**. As such it is rather casual and incomplete now. Sorry!

**Table of Contents**

* [Code Layout](#code-layout)
* [Parameter and Surge Synthesizer](#parameter-and-surge-synthesizer)
* [SurgeGUI](#surgegui)
* [Voices and MPE](#voices-and-mpe)
* [DSP](#dsp)
* [Hosts](#hosts)
* [VSTGUI](#vstgui)
* [Other Considerations](#other-considerations)

# Code Layout

Third party libraries are copied or submoduled into `libs`. VSTSDK and VSTGUI is an exception. They
are submodules in the top level directory. All of the surge C++ source is in the `src` directory.

`src/common` contains common code; `src/common/gui` and `src/common/dsp` contain the gui and dsp code
respectively.

Each of the hosts has a directory in `src`: `src/vst2`, `src/vst3`, `src/au` and `src/headless` are the
four flavors of host we have now.

# SurgeStorage, Parameter and Surge Synthesizer

The code collaboration between the parts of Surge happens through the
SurgeStorage class, which acts as a named bag of parameters. Each parameter
is represented by the Parameter class, which holds type and value and has
an update semantic. The SurgeSynthesizer at runtime reads the values from
the current SurgeStorage.

This means that things like loading a new patch amount to unstreaming SurgeStorage
into the synthesizers current storage instance. Similarly, changing a value in the UI
means locating the appropriate set of Parameters and calling the appropriate invalidating
value change function on them.

# SurgeGUI

SurgeGUI collaborates with the Parameter set as outlined above.

Critically it also has a ::idle thread which allows parameter changes from outside
the UI (like those delivered by the host) to be reflected in the UI.

# Voices and MPE

The surge voice allocation and stealing code is complicated, supporting multiple polyphony modes
and supporting MPE, which adds substantial complexity. The core data structure is a list of voices
which are allocated at startup and configured with an in-place contructor. Each has a reference
to state for the channel in which it is playing.

The voice class is in `src/common/dsp/SurgeVoice.h` and contains a `SurgeVoiceState`.    

# DSP

The most important thing about the DSP engine is it contains hand-coded SSE2 implementations of
most activities. This makes the engine fast, but also requires all data structures to align
on 16 byte boundaries.

# Hosts

The three traditional hosts (VST2, 3 and AU) are pretty straight forward.

The headless host makes an executable out of surge without a UI attached. Right now it is
primarily used for debugging and testing.

# VSTGUI

See (our documentation on how we use vstgui in surge)[vstgui-dev.md]

# Other Considerations.

## Why didn't you use 3rd party framework XYZ

Here's a summary of a discussion some of the devs had.

**JUCE**: JUCE makes adapting to different audio APIs easy, which we don't need because we
have our own code for that. It is plug-in framework that makes many things easy
at the price of flexibility.

**QT** Qt might be in a distant future an option worth of considering because we would
get rid alot of Cocoa and Windows specific code. In addition the flexibility
how things can be done would be compared to VSTGUI from a different dimension.
