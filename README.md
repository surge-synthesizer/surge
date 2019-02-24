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

Surge currently builds with Windows and macOS (AudioUnit, VST2, VST3), and getting it to function with Linux is under way but still requires some further effort.

There is currently work going on to create an official release-page with installers for Windows 64-bit, macOS 64-bit and other formats.

# System requirements

* At least Pentium 4 CPU.
* 64-bit Windows or macOS.

# Locations
## macOS

For user-made presets, the folder is `~/Documents/Surge`

For Surge factory presets, wavetables and preferences, the folder is `/Library/Application Support/Surge`

When installing globally (with the installer), the plugins will get installed into `/Library/Audio/Plug-Ins`.

If you install locally (with `./build-osx.sh`), the plugins will get installed into `~/Library/Audio/Plug-Ins`.

Having the same name plugin in both locations can cause conflicts, so do remember to wipe all `Surge.component`, `Surge.vst` and `Surge.vst3` instances from each path when reinstalling (globally) or when re-baking with `./build-osx.sh` (locally).

## Windows

For user-made presets, the location is `C:\Users\<yourusername>\Documents\Surge`

For Surge factory presets, wavetables and preferences, the folder is: `C:\Users\<yourusername>\AppData\Local\Surge`

# Windows

## Building with Windows

Prerequisites

* [Git](https://git-scm.com/downloads)
* [Premake 5](https://premake.github.io/download.html#v5) for generating project files
* [Visual Studio 15.5 (at least)](https://visualstudio.microsoft.com/downloads/)
* [Inno Setup](http://jrsoftware.org/isdl.php) for building the installer
* [nuget](https://www.nuget.org/downloads) for installing Nuget

Install Git, Premake5, Visual Studio 2017, Inno Setup and nuget. 

While Visual Studio 2017 is being installed, remember to select `C++` and `Windows 8.1 SDK` in the installer.

After all this is done, clone the repo and get the required submodules with the following commands.

```
git clone https://github.com/surge-synthesizer/surge.git
cd surge
git submodule update --init --recursive
```

Build with Windows by running

```
build.cmd
```

You can also, instead of running `build.cmd`, generate the project files by using

```
premake5 vs2017
```

After which you can open the freshly generated Visual Studio solution `Surge.sln` - If you had the `VST2.4SDK` folder specified prior to running `premake5`, you will have `surge-vst2.vcxproj` and `surge-vst3.vcxproj` in your Surge folder.

Now, to build the installer, open the file `installer_win/surge.iss` by using `Inno Setup`.

`Inno Setup` will bake an installer and place it in `installer_win/Output/`

As of Jan 2019, Microsoft is making free Windows 10 VM's available which contain their development tooling and are capable of building Surge at [this Microsoft page](https://developer.microsoft.com/en-us/windows/downloads/virtual-machines).

To setup, after starting the VM for the first time:
Run Visual Studio installer, update, and then make sure the `desktop C++ kit`, including `optional CLI support`, `Windows 8.1 SDK`, and `VC2015 toolset for desktop` is installed. 

Then proceed as above.

# macOS

## Build Pre-requisites

This process expects that you have both `Xcode` and `Xcode Command Line Utilities` installed. The commandline to install the `Xcode Command Line Utilities` is 

```
xcode-select --install
```

Install `premake5` by downloading it from  https://premake.github.io. Unzip the package. Install it into /usr/local/bin or elsewhere in your path.

```
cp premake5 /usr/local/bin
```

Clone the Surge repo to a path of your choice, enter it, and grab the submodules.

```
git clone https://github.com/surge-synthesizer/surge.git
cd surge
git submodule update --init --recursive
```

## Building with build-osx.sh

`build-osx.sh` has all the commands you need to build, test, locally install, validate, and package Surge on Mac.
It's what the primary Mac developers use day to day. The simplest approach is to build everything with

```
./build-osx.sh
```

this command will build, but not install, the VST3 and AU components. It has a variety of options which
are documented in the `./build-osx.sh --help` screen but a few key ones are:

* `./build-osx.sh --build-validate-au` will build the audio unit, correctly install it locally in `~/Library`
and run au-val on it. Running any of the installing options of `./build-osx` will install assets on your
system. If you are not comfortable removing an audio unit by hand and the like, please exercise caution.

* `./build-osx.sh --build-and-install` will build all assets and install them locally

* `./build-osx.sh --clean-all` will clean your work area of all assets

* `./build-osx.sh --clean-and-package` will do a complete clean, thena complete build, then build
a mac installer package which will be deposited in products.

`./build-osx.sh` is also impacted by a couple of environment variables.

* `VST2SDK_DIR` points to a vst2sdk. If this is set vst2 will build. If you set it after having
done a run without vst2, you will need to `./build-osx.sh --clean-all` to pick up vst2 consistently

* `BREWBUILD=TRUE` will use homebrew clang. If XCode refuses to build immediately with `error: invalid value 'c++17' in '-std=c++17'` 
and you do not want to upgrade XCode to a more recent version, use [homebrew](https://brew.sh/) to install llvm and set this variable.
 
## Using Xcode

`premake xcode4` builds xcode assets so if you would rather just use xcode, you can open the solutions created after running premake yourself or having `./build-osx.sh` run it.

After the build runs, be it successful or not, you can now launch Xcode and open the `Surge` folder. Let Xcode do it's own indexing / processing, which will take a while.

All of the three projects (`surge-vst3`, `surge-vst2`, `surge-au`) will recommend you to `Validate Project Settings`, meaning, more precisely, to `Update to recommended settings`. By clicking on `Update to recommended settings`, a dialog will open and you'll be prompted to `Perform Changes`. Perform the changes.

Xcode will result in built assets in `products/` but will not install them and will not install the ancilliary assets. To do that you can either `./build-osx.sh --install-local` or `./build-osx.sh --package` and run the resulting pkg file to install in `/Library`.

## Using the built assets

Once you have underaken the install options above, the AU and VSTs should just work. Launch your favorite host, get it to load the plugin, and have fun!

To use the AU in Logic, Mainstage, GarageBand, and so on, you need to do one more one-time step which is to invalidate your AU cache so that you force Logic to rescan. The easiest way to do this is to move the AudioUnitCache away from it's location by typing in:

```
mv ~/Library/Caches/AudioUnitCache ~/Desktop
```

then restart Logic and rescan. If the rescan succeeds you can remove the cache from `~/Desktop`.

Finally, the mac devs can't recomment [hosting au](http://ju-x.com/hostingau.html) highly enough for development. Open it once, open surge, and save that configuration to your desktop. You will quickly find that

```
./build-osx.sh --build-validate-au && /Applications/Hosting\ AU.app/Contents/MacOS/Hosting\ AU ~/Desktop/Surge.hosting 
```

is a super fast way to do a build and pop up Surge with stdout and stderr attached to your terminal.


## Packaging an installer

`./build-osx.sh --package` will create a `.pkg` file with a reasonable name. If you would like to use an alternate version number, though, the packaging script is in `installer_mac`.

```
cd installer_mac
./make_installer.sh myPeculiarVersion
open .
```

is an available option.

# Linux

## Building a Surge.vst (VST2) & Surge.vst3 (VST3) with Linux

Download `premake5` from https://premake.github.io/download.html#v5

Untar the package, and move it to `~/bin/` or elsewhere in your path so the install script can find it.

For VST2, you will need the `VST2 SDK` - unzip it to a folder of your choice and set `VST2SDK_DIR` to point to it.

```
export VST2SDK_DIR="/your/path/to/VST2SDK"
```

You will need to install a set of dependencies.

- build-essential
- libcairo-dev
- libxkbcommon-x11-dev
- libxkbcommon-dev
- libxcb-cursor-dev
- libxcb-keysyms1-dev
- libxcb-util-dev

Then `git clone` surge and update the submodules:

```
git clone https://github.com/surge-synthesizer/surge.git
cd surge
git submodule update --init --recursive
```

You can now build with the command

```
./build-linux.sh build
```

which will run premake and build the asset.

To use the VST, you need to install it locally along with supporting files. You can do this manually
if you desire, but the build script will also do it.

```
./build-linux.sh install-local
```

For other options, you can do `./build-linux.sh --help`.

## References

  * Most Surge-related conversation on the Surge Synth Slack. [You can join via this link](https://join.slack.com/t/surgesynth/shared_invite/enQtNTE4OTg0MTU2NDY5LTE4MmNjOTBhMjU5ZjEwNGU5MjExODNhZGM0YjQxM2JiYTI5NDE5NGZkZjYxZTkzODdiNTM0ODc1ZmNhYzQ3NTU)
  * IRC channel at #surgesynth at irc.freenode.net. The logs are available at https://freenode.logbot.info/surgesynth/.
  * Discussion at KVR-Forum [here](https://www.kvraudio.com/forum/viewtopic.php?f=1&t=511922)
