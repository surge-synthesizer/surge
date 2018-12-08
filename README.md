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

First you need to grab all git submodules (needed to get the VST3 SDK)

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

This process expects that you have Xcode and Xcode Command Line Utilities installed.

Install `premake5` by downloading it from  https://premake.github.io. Unzip the package.

Launch the Terminal in the folder where `premake5` has been unzipped into and copy it to `/usr/local/bin`.

```
cp premake5 /usr/local/bin
```
 
Clone the Surge repo to a path of your choice.

```
git clone https://github.com/kurasu/surge.git
```

Enter the Surge folder and use the following command to grab all the submodules referenced by Surge.

```
git submodule update --init --recursive
```

Execute the Surge build-script.

```
./build-osx.sh
```

If you got Xcode-Select issues or are missing the Command Line Utilities, grab them.

After the build runs, be it successful or not, you can now launch Xcode and open the `Surge` folder. Let Xcode do it's own indexing / processing, which takes a while.

The `surge-vst3 project` will now warn you to `Validate Project Settings`, meaning, more precisely, to `Update to recommended settings`. By clicking on `Update to recommended settings`, a dialog will open and you'll be prompted to `Perform Changes`. Perform the changes.

The `surge-au project` will also prompt to `Update to recommended settings` & `Perform Changes`, so, perform the changes.

After this we've reached the situation of "Here there be dragons" - Please, could anyone take this further?

## Building - VST2

If you want to build VST2 versions of the plug-in, set the environment variable VST24SDK to the location of the SDK prior to building.
