# Surge

CI: [![CI Build Status](https://dev.azure.com/surge-synthesizer/surge/_apis/build/status/surge-synthesizer.surge?branchName=main)](https://dev.azure.com/surge-synthesizer/surge/_build/latest?definitionId=2&branchName=main)
Release: [![Release Build Status](https://dev.azure.com/surge-synthesizer/surge/_apis/build/status/surge-synthesizer.releases?branchName=master)](https://dev.azure.com/surge-synthesizer/surge/_build/latest?definitionId=1&branchName=master)
Release-XT: [![Release-XT Build Status](https://dev.azure.com/surge-synthesizer/surge/_apis/build/status/surge-synthesizer.releases-xt?branchName=master)](https://dev.azure.com/surge-synthesizer/surge/_build/latest?definitionId=13&branchName=master)

Surge is an open-source digital synthesizer, originally written and sold as a commercial product
by @kurasu/Claes Johanson at [Vember Audio](http://vemberaudio.se). In September 2018,
Claes chose to release a partially completed version of Surge 1.6 under GPL3, and a group
of developers have been improving it since. You can learn more about the team at https://surge-synth-team.org/ or
connect with us on [Discord](https://raw.githubusercontent.com/surge-synthesizer/surge-synthesizer.github.io/master/_includes/discord_invite_link).

**If you are a musician only looking to use Surge, please download the appropriate binary
[from our website](https://surge-synthesizer.github.io). The Surge developer team makes regular releases for all supported platforms.**

If you would also like to participate in discussions, testing and design of Surge, we have
details below and also in [the contributors section of the Surge website](https://surge-synthesizer.github.io/#contributors).

In spring 2021, after the release of Surge 1.9, the Surge team embarked on a plan to replatform Surge as a JUCE plugin.
There are a variety of reasons for this choice, including the difficulty of maintaining hand wrappers aroudn VST3, AU and LV2
and limitations in the VSTGUI framework. As such *If you want to build Surge 1.9 or earlier, you need to check out the
repo on a branch.* **Document This Post Merge**

This README serves as the root of developer documentation for Surge.


# Developing Surge-XT

We welcome developers! Our workflow revolves around GitHub issues in this repository
and conversations at our Discord server and IRC chatroom. You can read our developer guidelines
in [our developer guide doc](doc/Developer%20Guide.md). If you want to contribute and are new to Git, 
we also have a [Git How To](doc/git-howto.md), tailored at Surge development.

The developer guide also contains information about testing and debugging in particular hosts
on particular platforms.

Surge-XT uses CMake for all of its build related tasks, and requires a set of free tools to build the synth. If you
have a development environment set up, you almost definitely have what you need, but if not please checkout:

- [Setup Build Environment on Windows](#windows)
- [Setup Build Environment on macOS](#macos)
- [Setup Build Environment on Linux](#linux)

Once you have set up your environment, you need to checkout the Surge code with git, grab submodules, run cmake
to configure, then run cmake to build.  Your IDE may support CMake (more on that below) but a reliable way to build
surge on all platforms is

```
git clone https://github.com/surge-synthesizer/surge.git
cd surge
git checkout xt-alpha
git submodule update --init --recursive
cmake -Bbuild 
cmake --build build --config Release --target surge-staged-assets
```

This will build all the surge binary assets in the directory `build/surge_xt_products` and is often enough of a formula
to do a build.

## Developing from your own fork

## Building projects for your IDE

## Building with support for VST2 or ASIO

## Building an LV2

On Linux, using a community fork of JUCE, you can build an LV2. Here's how. We assume you ahve checked out surge and can build.

First, to some directory of your chosing, clone https://github.com/lv2-porting-project/JUCE/tree/lv2 on branch lv2

```
sudo apt-get install -y lv2-dev
cd /some/location
git clone --branch lv2 https://github.com/lv2-porting-project/JUCE JUCE-lv2
```

then run a fresh CMake to (1) point to that juce and (2) activate LV2

```
cmake -Bbuild_lv2 -DCMAKE_BUILD_TYPE=Release -DJUCE_SUPPORTS_LV2=True -DSURGE_ALTERNATE_JUCE=/some/location/JUCE-lv2/
cmake --build build_lv2 --config Release --target surge-xt_LV2 --parallel 4
cmake --build build_lv2 --config Release --target surge-fx_LV2 --parallel 4
```

You will then have LV2s in `build_lv2/surge-xt_artefacts/Release/LV2` and  `build_lv2/surge-fx_artefacts/Release/LV2` 
respectively.

## Platform Specific Choices

### Building 32 vs 64 bit windows

### Building a mac FAT (arm/Intel) binary

### Building for Raspberry PI

*ToDo: Expand README documentation*


# Setting up for your OS

## Windows

You need to install the following

* [Visual Studio 2017, 2019, or later(version 15.5 or newer)](https://visualstudio.microsoft.com/downloads/)
* Install [Git](https://git-scm.com/downloads), [Visual Studio 2017 or newer](https://visualstudio.microsoft.com/downloads/)
* When you install Visual Studio, make sure to include CLI tools and CMake, which are included in
'Optional CLI support' and 'Toolset for desktop' install bundles

## macOS

To build on macOS, you need `Xcode`, `Xcode Command Line Utilities`, and CMake. Once you have installed
`Xcode` from the App Store, the command line to install the `Xcode Command Line Utilities` is:

```
xcode-select --install
```

There are a variety of ways to install CMake. If you use [homebrew](https://brew.sh) you can:

```
brew install cmake
```

## Linux

Most Linux systems have CMake, git and a modern C++ compiler installed. Make sure yours does.
We test with most gccs older than 7 or so and clangs after 9 or 10.
You will also need to install a set of dependencies:

- build-essential
- libcairo-dev
- libxkbcommon-x11-dev
- libxkbcommon-dev
- libxcb-cursor-dev
- libxcb-keysyms1-dev
- libxcb-util-dev

# Continuous Integration

In addition to the build commands above, we use Azure pipelines for continuous integration.
This means that each and every pull request will be automatically built in all our environments,
and a clean build on all platforms is an obvious pre-requisite. If you have questions about
our CI tools, don't hesitate to ask on our [Discord](https://raw.githubusercontent.com/surge-synthesizer/surge-synthesizer.github.io/master/_includes/discord_invite_link)
server. We are grateful to Microsoft for providing Azure pipelines for free to the open source community!

# References

  * Most Surge-related conversation happens on the Surge Synthesizer Discord server. [You can join via this link](https://raw.githubusercontent.com/surge-synthesizer/surge-synthesizer.github.io/master/_includes/discord_invite_link)
  * IRC channel at #surgesynth at irc.freenode.net. The logs are available at https://freenode.logbot.info/surgesynth/.
  * Discussion at KvR forum [here](https://www.kvraudio.com/forum/viewtopic.php?f=1&t=511922)
