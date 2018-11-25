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

[Releases are available here](https://github.com/kurasu/surge/releases)

Discussion at KVR-Forum [here](https://www.kvraudio.com/forum/viewtopic.php?f=1&t=511922)
Development Discussion at KVR-Forum [here](https://www.kvraudio.com/forum/viewtopic.php?f=33&t=511921)

## Preparation

First you need to grab all git submodules (needed to get the VST SDKs)

```
git submodule update --init --recursive
```

## Building - Windows

Prerequisites

* [Git](https://git-scm.com/downloads)
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

## Building - OSX

Start by grabbing `premake5` from https://premake.github.io . 

Copy `premake5` to `/usr/local/bin`

Clone the Surge repo by typing 

```
git clone https://github.com/kurasu/surge.git
```

After that, go into the Surge folder and get all the submodules referenced by the Surge repo by typing

```
git submodule update --init --recursive
```

Now, boot up Xcode and open the project. Let it do the indexing/processing.

Choose `Update to recommended settings" for `surge-au`, `surge-vst2` and `surge-vst3`. Click on `Perform Changes`.

After which "Here there be dragons" - Please, could anyone take this further?
