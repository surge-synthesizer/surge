# Surge XT

**If you are a musician looking to use Surge XT, please download the appropriate binary
[from our website](https://surge-synthesizer.github.io). Surge Synth Team makes regular releases for all supported
platforms.**

CI: [![CI Build Status](https://dev.azure.com/surge-synthesizer/surge/_apis/build/status/surge-synthesizer.surge?branchName=main)](https://dev.azure.com/surge-synthesizer/surge/_build/latest?definitionId=2&branchName=main)
Release: [![Release Build Status](https://dev.azure.com/surge-synthesizer/surge/_apis/build/status/surge-synthesizer.releases?branchName=master)](https://dev.azure.com/surge-synthesizer/surge/_build/latest?definitionId=1&branchName=master)
Release-XT: [![Release-XT Build Status](https://dev.azure.com/surge-synthesizer/surge/_apis/build/status/surge-synthesizer.releases-xt?branchName=master)](https://dev.azure.com/surge-synthesizer/surge/_build/latest?definitionId=13&branchName=master)

Surge XT is a free and open-source hybrid synthesizer, originally written and sold as a commercial product by @kurasu/Claes
Johanson at [Vember Audio](http://vemberaudio.se). In September 2018, Claes decided to release a partially completed
version of Surge 1.6 under GPL3, and a group of developers have been improving it since. You can learn more about the
team at https://surge-synth-team.org/ or connect with us
on [Discord](https://raw.githubusercontent.com/surge-synthesizer/surge-synthesizer.github.io/master/_includes/discord_invite_link)
.

If you would also like to participate in discussions, testing and design of Surge XT, we have details below and also in
the [contributors section of the Surge XT website](https://surge-synthesizer.github.io/#contributors).

This readme serves as the root of developer documentation for Surge XT.

# Developing Surge XT

We welcome developers! Our workflow revolves around GitHub issues in this repository and conversations at our Discord
server. You can read our developer guidelines
in [our developer guide document](doc/Developer%20Guide.md). If you want to contribute and are new to Git, we also have
a [Git How To](doc/How%20to%20Git.md), tailored at Surge XT development.

The developer guide also contains information about testing and debugging in particular hosts on particular platforms.

Surge XT uses CMake for all of its build-related tasks, and requires a set of free tools to build the synth. If you have
a development environment set up, you almost definitely have what you need, but if not, please check out:

- [Setting up Build Environment on Windows](#windows)
- [Setting up Build Environment on macOS](#macos)
- [Setting up Build Environment on Linux](#linux)

Once you have set your environment up, you need to checkout the Surge XT code with Git, grab submodules, run CMake to
configure, then run CMake to build. Your IDE may support CMake (more on that below), but a reliable way to build Surge XT
on all platforms is:

```
git clone https://github.com/surge-synthesizer/surge.git
cd surge
git submodule update --init --recursive
cmake -Bbuild
cmake --build build --config Release --target surge-staged-assets
```

This will build all the Surge XT binary assets in the directory `build/surge_xt_products` and is often enough of a formula
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

Surge XT developers regularly develop with all sorts of tools. CLion, Visual Studio, vim, emacs, VS Code, and many others
can work properly with the software.

## Building a VST2

Due to licensing restrictions, VST2 builds of Surge XT **may not** be redistributed. However, it is possible to build a
VST2 of Surge XT for your own personal use. First, obtain a local copy of the VST2 SDK, and unzip it to a folder of your
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

On Windows, building with ASIO is often preferred for Surge XT standalone, since it enables users to use the ASIO
low-latency audio driver.

Unfortunately, due to licensing conflicts, binaries of Surge XT that are built with ASIO **may not** be redistributed.
However, you can build Surge XT with ASIO for your own personal use, provided you do not redistribute those builds.

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

Surge XT 1.3 family moves to JUCE 7, which includes support for LV2 builds. For a variety of reasons
we don't build LV2 either by default or in our CI pipeline. You can activate the LV2 build in your
environment by adding `-DSURGE_BUILD_LV2=TRUE` on your initial CMake build.

## Building and Using the Python Bindings

Surge XT uses `pybind` to expose the innards of the synth to Python code for direct
native access to all its features. This is a tool mostly useful for developers,
and the [surge-python](https://github.com/surge-synthesizer/surge-python) 
repository shows some uses.

To use Surge XT in this manner, you need to build the Python extension. Here's
how (this shows the result on Mac, but Windows and Linux are similar).

First, configure a build with Python bindings activated:

```
cmake -Bignore/bpy -DSURGE_BUILD_PYTHON_BINDINGS -DCMAKE_BUILD_TYPE=Release
```

Note the directory `ignore/bpy` could be anything you want. The `ignore`
directory is handy, since it is ignored via `.gitignore`.

Then build the Python plugin:

```
cmake --build ignore/bpy --parallel --target surgepy
```

which should result in the Python .dll being present:

```bash
% ls ignore/bpy/src/surge-python/*so
ignore/bpy/src/surge-python/surgepy.cpython-311-darwin.so
```

Now you can finally start Python to load that. Here is an example interactive
session, but it will work similarly in the tool of your choosing:

```bash
% python3
Python 3.11.4 (main, Jun 20 2023, 17:37:48) [Clang 14.0.0 (clang-1400.0.29.202)] on darwin
Type "help", "copyright", "credits" or "license" for more information.
>>> import sys
>>> sys.path.append("ignore/bpy/src/surge-python")
>>> import surgepy
>>> surgepy.getVersion()
'1.3.main.850bd53b'
>>> quit()
```

## Building an Installer

The CMake target `surge-xt-distribution` builds an install image on your platform at the end of the build process. On
Mac and Linux, the installer generator is built into the platform; on Windows, our CMake file uses NuGet to download
InnoSetup, so you will need the [nuget.exe CLI](https://nuget.org/) in your path.

## Using CMake on the Command Line for More

We have a variety of other CMake options and targets which can allow you to develop and install Surge XT more easily.

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

Surge XT builds natively on 64-bit Raspberry Pi operating systems. Install your compiler
toolchain and run the standard CMake commands. Surge XT will *not* build on 32-bit Raspberry Pi
systems, giving an error in Spring Reverb and elsewhere in DSP code. If you would like to work
on fixing this, see the comment in CMakeLists.txt or drop us a line on our Discord or GitHub.

As of June 2023, though, gcc in some distributions has an apparent bug which generates a
specious warning which we promote to an error. We found Surge XT compiles cleanly with
`gcc (Debian 10.2.1-6) 10.2.1 20210110`, but not with others.
Surge XT also compiles with Clang 11. The error in question takes the form:

```
/home/pi/Documents/github/surge/libs/sst/sst-filters/include/sst/filters/QuadFilterUnit_Impl.h:539:26: error: requested alignment 16 is larger than 8 [-Werror=attributes]
     int DTi alignas(16)[4], SEi alignas(16)[4];
```

If you get that error and are working on RPi, your options are:

1. Change to a gcc version which doesn't mis-tag that as an error
2. Use Clang instead of gcc, as detailed below
3. Figure out how to suppress that error in CMake just for gcc on Raspberry Pi and send us a pull request

To build with Clang:

```bash
sudo apt install clang
cmake -Bignore/s13clang -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build ignore/s13clang --target surge-xt_Standalone --parallel 3
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

Surge XT cross-compiles to macOS Intel from Linux and BSD.

1. Install [osxcross](https://github.com/tpoechtrager/osxcross). Make sure to also install the `libclang_rt` library
   built by their `build_compiler_rt.sh` script.
2. Configure and build Surge XT:

```
cmake -DCMAKE_TOOLCHAIN_FILE=cmake/x86_64-apple-darwin20.4-clang.cmake -DCMAKE_FIND_ROOT_PATH=<path_to_osxcross_sdk> -Bbuild
cmake --build build
```

### Building older versions

Each version of Surge from 1.6 beta 6 or so has a branch in this repository. Just check it out and
read the associated README.

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

*You can find more info about Surge XT on Linux and other Unix-like distros in* [this document](doc/Linux-and-other-Unix-like-distributions.md).

# Continuous Integration

In addition to the build commands above, we use Azure pipelines for continuous integration. This means that each and
every pull request will be automatically built across all our environment,and a clean build on all platforms is an obvious
pre-requisite. If you have questions about our CI tools, don't hesitate to ask on
our [Discord](https://raw.githubusercontent.com/surge-synthesizer/surge-synthesizer.github.io/master/_includes/discord_invite_link)
server. We are grateful to Microsoft for providing Azure pipelines for free to the open-source community!

# References

* Most Surge XT-related conversation happens on the Surge Synthesizer Discord server. You can join
  via [this link](https://raw.githubusercontent.com/surge-synthesizer/surge-synthesizer.github.io/master/_includes/discord_invite_link).
* Discussion at KvR forum [here](https://www.kvraudio.com/forum/viewtopic.php?f=1&t=511922).
