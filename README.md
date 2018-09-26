# Surge

This is the synthesizer plug-in Surge which I previously sold as a commercial product as the company [vember audio](http://vemberaudio.se). 
As I'm too busy with [other](http://bitwig.com) projects and no longer want to put the effort into maintaining it myself across multiple platforms I have decided to give it new life as an open-source project.

It was originally released in 2005, and was one of my first bigger projects. The code could be cleaner, and at parts better explained but its reliable and sounds great. And beware, there might still be a few comments in Swedish.

The codebase was migrated from before an unfinished 1.6 release which improves on the last released 1.5.3 in a number of ways:

* Using a newer version of the [VSTGUI](https://github.com/steinbergmedia/vstgui) framework
  * This has caused a lot of graphical bugs, with some that still need to be fixed
  * But will enable a port to both 64-bit macOS and Linux
* Support for [VST3](https://www.steinberg.net/en/company/technologies/vst3.html)
* Support for [MPE](https://www.midi.org/articles-old/midi-polyphonic-expression-mpe)
* New analog mode for the ADSR envelopes   

It currently only builds on windows, but getting it to build on macOS again & Linux should be doable with moderate effort.

## Preparation

First you need to grab all submodules (needed to get the VST3DSK)

```
git submodule update --init --recursive
```

The [VST3SDK](https://github.com/steinbergmedia/vst3sdk) hosted by Steinberg on github doesn't contain the VST2 SDK bits, so if you want to build the VST2 version you need to [download](https://www.steinberg.net/vst3sdk) that one manually and add those missing files, they have a script that copies the copying for you.

## Building - Windows

Prerequisites

* [Premake 5](https://premake.github.io/download.html#v5) for generating project files
* [Visual Studio 2017](https://visualstudio.microsoft.com/downloads/)
* [Inno Setup](http://jrsoftware.org/isdl.php) for building the installer

To build on windows:

```
build.cmd
```

Or you can just generate the project files using

```
premake5 vs2017
```

and open the visual studio solution which is generated.

To build the installer open the file installer_win/surge.iss using Inno Setup.
