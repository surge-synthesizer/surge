# Surge XT

**If you are a musician looking to use Surge, please download the appropriate binary
[from our website](https://surge-synthesizer.github.io). Surge Synth Team makes regular releases for all supported
platforms.**

**Over 2021, after the successful release of Surge 1.9, the Surge Synth team undertook an effort to rebuild Surge in JUCE
and add a variety of features. This project resulted in the new version of Surge, called 'Surge XT'. The head of this
repo contains the current version of Surge XT. If you are looking to compile a stable production version of Surge, we
tag each release, so if you want to compile Surge 1.9, please check out the `release_1.9.0` tag.**

CI: [![CI Build Status](https://dev.azure.com/surge-synthesizer/surge/_apis/build/status/surge-synthesizer.surge?branchName=main)](https://dev.azure.com/surge-synthesizer/surge/_build/latest?definitionId=2&branchName=main)
Release: [![Release Build Status](https://dev.azure.com/surge-synthesizer/surge/_apis/build/status/surge-synthesizer.releases?branchName=master)](https://dev.azure.com/surge-synthesizer/surge/_build/latest?definitionId=1&branchName=master)
Release-XT: [![Release-XT Build Status](https://dev.azure.com/surge-synthesizer/surge/_apis/build/status/surge-synthesizer.releases-xt?branchName=master)](https://dev.azure.com/surge-synthesizer/surge/_build/latest?definitionId=13&branchName=master)

Surge is a free and open-source hybrid synthesizer, originally written and sold as a commercial product by @kurasu/Claes
Johanson at [Vember Audio](http://vemberaudio.se). In September 2018, Claes decided to release a partially completed
version of Surge 1.6 under GPL3, and a group of developers have been improving it since. You can learn more about the
team at https://surge-synth-team.org/ or connect with us
on [Discord](https://raw.githubusercontent.com/surge-synthesizer/surge-synthesizer.github.io/master/_includes/discord_invite_link)
.

If you would also like to participate in discussions, testing and design of Surge, we have details below and also in
the [contributors section of the Surge website](https://surge-synthesizer.github.io/#contributors).

This readme serves as the root of developer documentation for Surge.

# Developing Surge XT

We welcome developers! Our workflow revolves around GitHub issues in this repository and conversations at our Discord
server. You can read our developer guidelines
in [our developer guide document](doc/Developer%20Guide.md). If you want to contribute and are new to Git, we also have
a [Git How To](doc/How%20to%20Git.md), tailored at Surge development.

The developer guide also contains information about testing and debugging in particular hosts on particular platforms.

Surge XT uses CMake for all of its build-related tasks, and requires a set of free tools to build the synth. If you have
a development environment set up, you almost definitely have what you need, but if not, please check out:

- [Setting up Build Environment on Windows](#windows)
- [Setting up Build Environment on macOS](#macos)
- [Setting up Build Environment on Linux](#linux)

Once you have set your environment up, you need to checkout the Surge code with Git, grab submodules, run CMake to
configure, then run CMake to build. Your IDE may support CMake (more on that below), but a reliable way to build Surge
on all platforms is:

```
git clone https://github.com/surge-synthesizer/surge.git
cd surge
git submodule update --init --recursive
cmake -Bbuild
cmake --build build --config Release --target surge-staged-assets
```

This will build all the Surge binary assets in the directory `build/surge_xt_products` and is often enough of a formula
to do a build.

## Developing from your own fork

Our [Git How To](doc/How%20to%20Git.md) explains how we are using Git. If you want to develop from your own fork, please
consult there, but the short version is (1) fork this project on GitHub and (2) clone your fork, rather than the main
repo as described above. So press the `Fork` button here and then:

```
git clone git@github.com:youruserid/surge.git
```

and the rest of the steps are unchanged.

## Building projects for your IDE

When you run the first CMake step, CMake will generate IDE-compatible files for you. On Windows, it will generate Visual
Studio files. On Mac it will generate makefiles by default, but if you add the argument `-GXcode` you can get an XCode
project if you want.

Surge developers regularly develop with all sorts of tools. CLion, Visual Studio, vim, emacs, VS Code, and many others
can work properly with the software.

## Building a VST2

Due to licensing restrictions, VST2 builds of Surge **may not** be redistributed. However, it is possible to build a
VST2 of Surge for your own personal use. First, obtain a local copy of the VST2 SDK, and unzip it to a folder of your
choice. Then set `VST2SDK_DIR` to point to that folder:

```
export VST2SDK_DIR="/your/path/to/VST2SDK"
```

or, in the Windows command prompt:

```
set VST2SDK_DIR=c:\path\to\VST2SDK
```

Finally, run CMake afresh, and build the VST2 targets:

```
cmake -Bbuild_vst2
cmake --build build_vst2 --config Release --target surge-xt_VST --parallel 4
cmake --build build_vst2 --config Release --target surge-fx_VST --parallel 4
```

You will then have VST2 plugins in `build_vst2/surge-xt_artefacts/Release/VST`
and  `build_vst2/surge-fx_artefacts/Release/VST` respectively. Adjust the number of cores that will be used for building
process by modifying the value of `--parallel` argument.

## Building with support for ASIO

On Windows, building with ASIO is often preferred for Surge standalone, since it enables users to use the ASIO
low-latency audio driver.

Unfortunately, due to licensing conflicts, binaries of Surge that are built with ASIO **may not** be redistributed.
However, you can build Surge with ASIO for your own personal use, provided you do not redistribute those builds.

If you already have a copy of the ASIO SDK, simply set the following environment variable and you're good to go!

```
set ASIOSDK_DIR=c:\path\to\asio
```

If you DON'T have a copy of the ASIO SDK at hand, CMake can download it for you, and allow you to build with ASIO under your
own personal license. To enable this functionality, run your CMake configuration command as follows:

```
cmake -Bbuild -DBUILD_USING_MY_ASIO_LICENSE=True
```

## Building an LV2

On Linux, using a community fork of JUCE, you can build an LV2. Here's how! We assume you have checked out Surge and can
build.

First, clone https://github.com/lv2-porting-project/JUCE/tree/lv2 on branch lv2, to some directory of your choosing:

```
sudo apt-get install -y lv2-dev
cd /some/location
git clone --branch lv2 https://github.com/lv2-porting-project/JUCE JUCE-lv2
```

Then run a fresh CMake to (1) point to that JUCE fork and (2) activate LV2:

```
cmake -Bbuild_lv2 -DCMAKE_BUILD_TYPE=Release -DJUCE_SUPPORTS_LV2=True -DSURGE_JUCE_PATH=/some/location/JUCE-lv2/
cmake --build build_lv2 --config Release --target surge-xt_LV2 --parallel 4
cmake --build build_lv2 --config Release --target surge-fx_LV2 --parallel 4
```

You will then have LV2s in `build_lv2/src/surge-xt/surge-xt_artefacts/Release/LV2`
and  `build_lv2/src/surge-xt/surge-fx_artefacts/Release/LV2` respectively.

## Building an Installer

The CMake target `surge-xt-distribution` builds an install image on your platform at the end of the build process. On
Mac and Linux, the installer generator is built into the platform; on Windows, our CMake file uses NuGet to download
InnoSetup, so you will need the [nuget.exe CLI](https://nuget.org/) in your path.

## Using CMake on the Command Line for More

We have a variety of other CMake options and targets which can allow you to develop and install Surge more easily.

### Plugin Development

JUCE supports a mode where a plugin (AU, VST3, etc...) is copied to a local install area after a build. This is off by
default with CMake, but you can turn it on with `-DSURGE_COPY_AFTER_BUILD=True` at `cmake` time.
If you do this on Unixes, building the VST3 or AU targets will copy them to the appropriate local area
(`~/.vst3` on Linux, `~/Library/Audio/Plugins` on Mac). On Windows it will attempt to install the VST3, so setting this
option may require administrator privileges in your build environment.

### CMake Install Targets (Linux and other non-Apple Unixes only)

On systems which are `UNIX AND NOT APPLE`, the CMake file provides an install target which will install all needed
assets to the `CMAKE_INSTALL_PREFIX`. This means a complete install can be accomplished by:

```
cmake -Bignore/sxt -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build ignore/sxt --config Release --parallel 8
sudo cmake --install ignore/sxt
```

and you should get a working install in `/usr/bin`, `/usr/share` and `/usr/lib`.

## Platform Specific Choices

### Building 32- vs 64-bit on Windows

If you are building with Visual Studio 2019, use the `-A` flag in your CMake command to specify 32/64-bit:

```bash
# 64-bit
cmake -Bbuild -G"Visual Studio 16 2019" -A x64

# 32-bit
cmake -Bbuild32 -G"Visual Studio 16 2019" -A Win32
```

If you are using an older version of Visual Studio, you must specify your preference with your choice of CMake generator:

```bash
# 64-bit
cmake -Bbuild -G"Visual Studio 15 2017 Win64"

# 32-bit
cmake -Bbuild32 -G"Visual Studio 15 2017"
```

### Building a Mac Fat Binary (ARM/Intel)

To build a fat binary on a Mac, simply add the following CMake argument to your initial CMake run:

```
-D"CMAKE_OSX_ARCHITECTURES=arm64;x86_64"
```

### Building for Raspberry Pi

To build for a Raspberry Pi, you want to add the `LINUX_ON_ARM` CMake variable when you first run CMake. Otherwise, the
commands are unchanged. So, on a Pi, you can do:

```
cmake -Bbuild -DLINUX_ON_ARM=True
cmake --build build --config Release --target surge-staged-assets
```

### Cross-compiling for aarch64

To cross-compile for aarch64, use the CMake Linux toolchain for aarch64, as shown in the Azure pipeline here:

```
cmake -Bignore/xc64 -DCMAKE_TOOLCHAIN_FILE=cmake/linux-aarch64-ubuntu-crosscompile-toolchain.cmake -DCMAKE_BUILD_TYPE=DEBUG -GNinja
cmake --build ignore/xc64 --config Debug --target surge-testrunner
```

Of course, that toolchain makes specific choices. You can make other choices as long as (1) you set the CMake variable
`LINUX_ON_ARM` and (2) you make sure your host and your target compiler are both 64-bit.

### Cross-compiling for macOS

Surge cross-compiles to macOS Intel from Linux and BSD.

1. Install [osxcross](https://github.com/tpoechtrager/osxcross). Make sure to also install the `libclang_rt` library
   built by their `build_compiler_rt.sh` script.
2. Configure and build Surge:

```
cmake -DCMAKE_TOOLCHAIN_FILE=cmake/x86_64-apple-darwin20.4-clang.cmake -DCMAKE_FIND_ROOT_PATH=<path_to_osxcross_sdk> -Bbuild
cmake --build build
```

# Setting up for Your OS

## Windows

You need to install the following:

* [Visual Studio 2017, 2019, or later(version 15.5 or newer)](https://visualstudio.microsoft.com/downloads/)
* Install [Git](https://git-scm.com/downloads)
  , [Visual Studio 2017 or newer](https://visualstudio.microsoft.com/downloads/)
* When you install Visual Studio, make sure to include CLI tools and CMake, which are included in
  'Optional CLI support' and 'Toolset for desktop' install bundles.

## macOS

To build on macOS, you need `Xcode`, `Xcode Command Line Utilities`, and CMake. Once you have installed
`Xcode` from the App Store, the command line to install the `Xcode Command Line Utilities` is:

```
xcode-select --install
```

There are a variety of ways to install CMake. If you use [homebrew](https://brew.sh), you can:

```
brew install cmake
```

## Linux

Most Linux systems have CMake, Git and a modern C++ compiler installed. Make sure yours does. We test with most gccs
older than 7 or so and clangs after 9 or 10. You will also need to install a set of dependencies. If you use `apt`, do:

```bash
sudo apt install build-essential libcairo-dev libxkbcommon-x11-dev libxkbcommon-dev libxcb-cursor-dev libxcb-keysyms1-dev libxcb-util-dev libxrandr-dev libxinerama-dev libxcursor-dev libasound2-dev libjack-jackd2-dev
```

*You can find more info about Surge on Linux and other Unix-like distros in* [this document](doc/Linux-and-other-Unix-like-distributions.md).

# Continuous Integration

In addition to the build commands above, we use Azure pipelines for continuous integration. This means that each and
every pull request will be automatically built across all our environments, and a clean build on all platforms is an obvious
pre-requisite. If you have questions about our CI tools, don't hesitate to ask on
our [Discord](https://raw.githubusercontent.com/surge-synthesizer/surge-synthesizer.github.io/master/_includes/discord_invite_link)
server. We are grateful to Microsoft for providing Azure pipelines for free to the open-source community!

# References

* Most Surge-related conversation happens on the Surge Synthesizer Discord server. You can join
  via [this link](https://raw.githubusercontent.com/surge-synthesizer/surge-synthesizer.github.io/master/_includes/discord_invite_link).
* Discussion at KvR forum [here](https://www.kvraudio.com/forum/viewtopic.php?f=1&t=511922).
