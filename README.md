# Surge

This is the synthesizer plug-in Surge which I previously sold as a commercial product as the company [vember audio](http://vemberaudio.se).

As I (@kurasu / Claes Johanson) am too busy with [other](http://bitwig.com) projects and no longer want to put the effort into maintaining it myself across multiple platforms, I have decided to give it new life as an open-source project.

It was originally released in 2005, and was one of my first bigger projects. The code could be cleaner, and certain parts better explained, but it is reliable and sounds great.

The codebase was migrated from before an unfinished 1.6 release, which improves on the last released 1.5.3 in a number of ways:

* It uses a newer version of the [VSTGUI](https://github.com/steinbergmedia/vstgui) framework
  * This has caused a lot of graphical bugs, with some still needing to be fixed
  * But it will enable the creation of a port for both 64-bit macOS and Linux
* Support for [VST3](https://www.steinberg.net/en/company/technologies/vst3.html)
* Support for [MPE](https://www.midi.org/articles-old/midi-polyphonic-expression-mpe)
* New analog mode for the ADSR envelopes   

Surge currently builds with Windows and macOS (AudioUnit,VST2), and getting it to build on Linux again should be doable with some effort.

[Releases available here](https://github.com/kurasu/surge/releases) - this page currently has Windows builds only.

## Preparation

First you need to grab all of the git submodules (these are needed to get the VST3 SDK)

```
git submodule update --init --recursive
```

## Building with Windows

Prerequisites

* [Git](https://git-scm.com/downloads)
* [Premake 5](https://premake.github.io/download.html#v5) for generating project files
* [Visual Studio 2017](https://visualstudio.microsoft.com/downloads/)
* [Inno Setup](http://jrsoftware.org/isdl.php) for building the installer

To build with Windows, run

```
build.cmd
```

Or you can just generate the project files by using

```
premake5 vs2017
```

After which you can open the Visual Studio solution which has been generated.

To build the installer, open the file `installer_win/surge.iss` using `Inno Setup`.

## Building a Surge.component (Audio Unit) with OSX/macOS

This process expects that you have both `Xcode` and `Xcode Command Line Utilities` installed.

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

Execute the Surge build script.

```
./build-osx.sh
```

If you see any issues with `Xcode-Select`, or you get told that you are missing the `Command Line Utilities`, grab them. The error itself should list the commands required.

After the build runs, be it successful or not, you can now launch Xcode and open the `Surge` folder. Let Xcode do it's own indexing / processing, which will take a while.

All of the three projects (`surge-vst3`, `surge-vst2`, `surge-au`) will recommend you to `Validate Project Settings`, meaning, more precisely, to `Update to recommended settings`. By clicking on `Update to recommended settings`, a dialog will open and you'll be prompted to `Perform Changes`. Perform the changes.

At this point you can build an Audio Unit, and it will link and pass validation.

To try the Audio Unit you will need to install and validate it. If you don't know how to disable and revalidate audio units, be 
a bit cautious here. You can slightly mess things up. To make it easy there's a script which will allow you to do this from the command line:

```
./installer-local-au.sh
```

This will update the build date, run a build, and if the build works, remove the version of surge in `/Library/Audio/Plug-ins`, replacing it with the latest AudioUnit. 
After that, the script will run `auvaltool` to make sure that the Audio Unit is properly installed.
You should be able to see the build date and time on `stderr` in the `auval` output.

Tips on how to develop and debug using this are in [this issue](https://github.com/kurasu/surge/issues/58).

You have successfully built and installed the AU if you see:

```
--------------------------------------------------
AU VALIDATION SUCCEEDED.
--------------------------------------------------
```

To use the AU in Logic, Mainstage, GarageBand, and so on, you need to do one more one-time step which is to invalidate your AU cache so that you force Logic to rescan. The easiest way to do this is to move the AudioUnitCache away from it's location by typing in:

```
mv ~/Library/Caches/AudioUnitCache ~/Desktop
```

After this, launch Logic. If everything works and starts up again, you can delete the cache from your desktop. If this doesn't succeed, you can always put it back again.

## Building a Surge.vst (VST2) with OSX/macOS

If you want to build VST2 versions of the plug-in, set the environment variable VST2SDK_DIR to the location of the SDK prior to building.

An example of setting the environment variable `VST2SDK_DIR` would be:

```export VST2SDK_DIR=~/programming/VST_SDK_2.4```

***NOTE***: This environment variable needs to be set _before_ running `premake5 xcode4` - which generates projects / and is part of the `build-osx.sh` script.

## Building a Surge.vst3 (VST3) with OSX/macOS

Here there be dragons

## Building a Surge.vst (VST2) with Linux

Some discussion at https://github.com/kurasu/surge/issues/19

## Building a Surge.vst3 (VST3) with Linux

Some discussion at https://github.com/kurasu/surge/issues/19

## References

  * Discussion at KVR-Forum [here](https://www.kvraudio.com/forum/viewtopic.php?f=1&t=511922)
  * IRC channel #surgesynth @ irc.freenode.net
