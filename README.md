# Surge

This is the synthesizer plug-in Surge which I previously sold as a commercial product as the company vember audio. 
As I'm to busy with other projects and no longer want to put the effort into maintaining it myself across multiple platforms I have decided to give it new life as an open-source project.

It was originally released in 2005, and was one of my first bigger projects. The code could be cleaner, and at parts better explained but its reliably and sounds great.

The codebase was migrated from before an unfinished 1.6 release which improves on 1.5.3 in a number of ways:

* Using a newer version of the VSTGUI framework
  * This has caused a lot of graphical bugs, with some that still need to be fixed
  * But will enable a port to both 64-bit macOS and Linux
* Support for VST3
* Support for VST3 Note Expressions
* New filter Zero-delay feedback filters
* New analog mode for the ADSR envelopes   

It currently only builds on windows, but getting it to work on macOS again & Linux should be doable with moderate effort.

## Preparation

First you need to grab all submodules (needed to get the VST3DSK)

```
git submodule update --init --recursive
```

The VST3SDK hosted by steinberg on github doesn't contain the VST2 SDK bits, so if you want to build the VST2 version you need to download that one manually and add those missing files.

## Building - Windows

Prerequisites

* Premake 4 (for generating project files)
* Visual Studio 2017 (Community edition is fine)

To build on windows:

```
build.cmd
```

Or you can just generate the project files using

```
premake4 vs2012
```

and open the visual studio solution which is generated.