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

Surge currently builds with Windows and macOS (AudioUnit,VST2,VST3), and getting it to build on Linux again should be doable with some effort.

Daily macOS (64-bit, AU/VST2/VST3) builds available on the [Slack](https://join.slack.com/t/surgeteamworkspace/shared_invite/enQtNTE3NDIyMDc2ODgzLTU1MzZmMWZlYjkwMjk4NDY4ZjI3NDliMTFhMTZiM2ZmNjgxNjYzNGI0NGMxNTk2ZWJjNzgyMDcxODc2ZjZmY2Q)

There is currently work going on to create an official release-page with installers for Windows 64-bit, macOS 64-bit and other formats.

## Preparation

First you need to grab all of the git submodules (these are needed to get the VST3 SDK)

```
git submodule update --init --recursive
```

# Windows

## Building with Windows

Prerequisites

* [Git](https://git-scm.com/downloads)
* [Premake 5](https://premake.github.io/download.html#v5) for generating project files
* [Visual Studio 15.5 (at least)](https://visualstudio.microsoft.com/downloads/)
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

As of Jan 2019, Microsoft is making free Windows 10 VM's available which contain their development tooling 
and are capable of building Surge
at [this Microsoft page](https://developer.microsoft.com/en-us/windows/downloads/virtual-machines).
To setup, after starting the VM for the first time
run visual studio installer, update, and then
make sure the desktop C++ kit, including optional CLI support, Windows 8.1 SDK, and vc2015 toolset for desktop is installed. 
Then proceed as above.

# macOS

## Build Pre-requisites

This process expects that you have both `Xcode` and `Xcode Command Line Utilities` installed.

Install `premake5` by downloading it from  https://premake.github.io. Unzip the package. Install it
in /usr/local/bin or elsewhere in your path.

```
cp premake5 /usr/local/bin
```

Clone the Surge repo to a path of your choice, and init submodules

```
git clone https://github.com/surge-synthesizer/surge.git
cd surge
git submodule update --init --recursive
```

## Building with build-osx.sh

`build-osx.sh` has all the commands you need to build, test, locally install, validate, and package Surge on mac.
It's what the primary mac developers use day to day. The simplest approach is to build everything

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

`premake xcode4` builds xcode assets so if you would rather just use xcode, you can open the solutions created after running premake yourself
or having `./build-osx.sh` run it.

After the build runs, be it successful or not, you can now launch Xcode and open the `Surge` folder. Let Xcode do it's own indexing / processing, which will take a while.

All of the three projects (`surge-vst3`, `surge-vst2`, `surge-au`) will recommend you to `Validate Project Settings`, meaning, more precisely, to `Update to recommended settings`. By clicking on `Update to recommended settings`, a dialog will open and you'll be prompted to `Perform Changes`. Perform the changes.

XCode will result in built assets in `products/` but will not install them and will not install the ancilliary assets. To do that you can either `./build-osx.sh --install-local` or
`./build-osx.sh --package` and run the resulting pkg file to install in `/Library`.

## Using the built assets

Once you have underaken the install options above, the AU and VSTs should just work. Launch your favorite host, get it to load the plugin, and have fun!

To use the AU in Logic, Mainstage, GarageBand, and so on, you need to do one more one-time step which is to invalidate your AU cache so that you force 
Logic to rescan. The easiest way to do this is to move the AudioUnitCache away from it's location by typing in:

```
mv ~/Library/Caches/AudioUnitCache ~/Desktop
```

then restart Logic and rescan. If the rescan succeeds you can remove the cache from `~/Desktop`.

Finally, the mac devs can't recomment [hosting au](http://ju-x.com/hostingau.html) highly enough for development. Open it once, open surge, and save that
configuration to your desktop. You will quickly find that

```
./build-osx.sh --build-validate-au && /Applications/Hosting\ AU.app/Contents/MacOS/Hosting\ AU ~/Desktop/Surge.hosting 
```

is a super fast way to do a build and pop up Surge with stdout and stderr attached to your terminal.


## Packaging an installer

`./build-osx.sh --package` will create a `.pkg` file with a reasonable name. If you would like to use an alternate version number, though, the packaging script is in `installer_mac`.

```
cd installer_mac
./make_installer.sh myPeculiarVersion
mv installer/Surge-myPeculiar-Version-Setup.pkg wherever-youwant
```

is an available option.

# Linux

## Building a Surge.vst (VST2) with Linux

Some discussion at https://github.com/surge-synthesizer/surge/issues/19

## Building a Surge.vst3 (VST3) with Linux

Some discussion at https://github.com/surge-synthesizer/surge/issues/19

## References

  * Discussion at KVR-Forum [here](https://www.kvraudio.com/forum/viewtopic.php?f=1&t=511922)
  * The official IRC channel is #surgesynth at irc.freenode.net. The logs are available at https://freenode.logbot.info/surgesynth/.
  * Some folks are on the SurgeSynth slack [which you can join here](https://join.slack.com/t/surgeteamworkspace/shared_invite/enQtNTE3NDIyMDc2ODgzLTU1MzZmMWZlYjkwMjk4NDY4ZjI3NDliMTFhMTZiM2ZmNjgxNjYzNGI0NGMxNTk2ZWJjNzgyMDcxODc2ZjZmY2Q)
