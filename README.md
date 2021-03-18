# Surge

CI: [![CI Build Status](https://dev.azure.com/surge-synthesizer/surge/_apis/build/status/surge-synthesizer.surge?branchName=main)](https://dev.azure.com/surge-synthesizer/surge/_build/latest?definitionId=2&branchName=main)
Release: [![Release Build Status](https://dev.azure.com/surge-synthesizer/surge/_apis/build/status/surge-synthesizer.releases?branchName=master)](https://dev.azure.com/surge-synthesizer/surge/_build/latest?definitionId=1&branchName=master)

Surge is an open-source digital synthesizer, originally written and sold as a commercial product
by @kurasu/Claes Johanson at [Vember Audio](http://vemberaudio.se). In September 2018,
Claes chose to release a partially completed version of Surge 1.6 under GPL3, and a group
of developers have been improving it since. You can learn more about the team at https://surge-synth-team.org/ or
connect with us on [Discord](https://raw.githubusercontent.com/surge-synthesizer/surge-synthesizer.github.io/master/_includes/discord_invite_link).

**If you are a musician only looking to use Surge, please download the appropriate binary
[from our website](https://surge-synthesizer.github.io). The Surge developer team makes regular releases for all supported platforms.**

If you would also like to participate in discussions, testing and design of Surge, we have
details below and also in [the contributors section of the Surge website](https://surge-synthesizer.github.io/#contributors).

Surge currently builds on macOS as a 64-bit AU, VST2 and VST3, Windows as a 64- and 32-bit VST2 and VST3,
and Linux as a 64-bit VST2, VST3 and LV2. We provide binary distributions of the AU and VST3 on our webpage.

This README serves as the root of developer documentation for Surge.

# Developing Surge

We welcome developers! Our workflow revolves around GitHub issues in this repository
and conversations at our Discord server and IRC chatroom. You can read our developer guidelines
in [our developer guide doc](doc/Developer%20Guide.md).

The developer guide also contains information about testing and debugging in particular hosts
on particular platforms.

We are also in the process of writing a [guide to the internal architecture of Surge](doc/Architecture.md), which
can help you get oriented and answer some basic questions.

If you want to contribute and are new to Git, we also have a [Git How To](doc/git-howto.md), tailored at Surge development.

# Building Surge

As of April 2020, Surge is built using CMake. If you are familiar with CMake, you can
jump to the <a href="#cmake-targets">CMake Targets</a> section.

## Windows

### Additional pre-requisites:

* [Visual Studio 2017 (version 15.5 or newer)](https://visualstudio.microsoft.com/downloads/)
* [Inno Setup](http://jrsoftware.org/isdl.php) for building the installer

### Install pre-requisites

* Install [Git](https://git-scm.com/downloads), [Visual Studio 2017 or newer](https://visualstudio.microsoft.com/downloads/)
* If you want to build an installer, install [Inno Setup](https://jrsoftware.org/isdl.php#stable)
* When you install Visual Studio, make sure to include CLI tools and CMake, which are included in
'Optional CLI support' and 'Toolset for desktop' install bundles

### Check out the code

* Log into your GitHub account and fork this repository
* Open Visual Studio's command prompt. This is done by running `x64 Native Tools Command Prompt for VS 2017/2019` (or similar) which is installed along with Visual Studio.
To do this, press Win key and start typing 'cmd', the command should pop up right away.
* In that command prompt, navigate to a writable directory where you want to check out Surge repository you've just forked
* Continuing in that same command prompt, run these commands one by one to check out the code:
```
git clone https://github.com/{your-user-name}/surge.git
cd surge
git submodule update --init --recursive
```

### Your first build

All of the following commands take place in VS command prompt as above.

* If you have the VST2 SDK and want to build the VST2 plugin, set the path to it as a user environment variable. If not, don't worry.
You can build the VST3 on Windows without any extra assets (we recommend all Windows users to
use the VST3). If you don't want to set up an environment variable, you can tell CMake where the path to VST2 SDK is like so:

```
set VST2SDK_DIR=c:\path\to\vst2
```

* Now, run CMake to create a build directory:

```
cmake . -Bbuild -A x64
```

* Choose to build in Visual Studio or the command line
   * To build in Visual Studio, open the file `build\Surge.sln` created in the previous step. The Internet is full
     of introductions to help with Visual Studio to take it from here.
   * To build from the VS command prompt, type:

```
cmake --build build --config Release --target Surge-VST3-Packaged
```

* After a successful build, the folder `build\surge_products` will contain `Surge.vst3`, which you can use to replace
  Surge in VST plugin path of your DAW. Note: if you have never installed Surge, you will also need to install the assets!
  You can do this by either downloading an official installer from our website, or by
  <a href="#building-the-windows-installer">creating an installer using Inno Setup</a>, then running it.

### Your first 32-bit build

* 32-bit build is done exactly like 64-bit build, but when you run CMake, choose a different target:

```
cmake . -Bbuild32 -A Win32
```

* To build the plugin, either open `build32\Surge.sln` or run the CMake build command with `build32` as the directory:

```
cmake --build build32 --config Release --target Surge-VST3-Packaged
```

### Building the Windows installer


If you want to build the installer, open the file `installer_win\surge.iss` with [Inno Setup](https://jrsoftware.org/isdl.php#stable).
Inno Setup will make an installer and place it in `installer_win\Output\`

### Building with Clang

If you have clang installed with vs2019, you can do

```
cmake -Bbuildclang -GNinja -DCMAKE_TOOLCHAIN_FILE=cmake/x86_64-w64-clang.cmake
```

and get a buildable set of ninja files. Our release pipeline may
shift to this soon.

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

Once that's done, fork this repository, clone it, and update submodules.

```
git clone https://github.com/{your-user-name}/surge.git
cd surge
git submodule update --init --recursive
```

### Building with build-osx.sh

`build-osx.sh` has all the commands you need to build, test, locally install, validate, and package Surge on Mac.
As of April 2020, it is a very thin wrapper on CMake and Xcode.
It's what the primary Mac developers use day to day. The simplest approach is to build everything with:

```
./build-osx.sh
```

`build-osx.sh` will give you a better output if you first `gem install xcpretty`, the Xcode build formatter,
and you have your `gem` environment running. If that doesn't work, don't worry - you can still build.

This command will build, but not install, the VST3 and AU components. It has a variety of options which
are documented in the `./build-osx.sh --help` screen, but a few key ones are:

* `./build-osx.sh --build-validate-au` will build the AU plugin, correctly install it locally in `~/Library`
and run au-val on it. Running any of the install options of `./build-osx` will install assets on your
system. If you are not comfortable removing an AU plugin by hand and the like, please exercise caution!

* `./build-osx.sh --build-and-install` will build all assets and install them locally.

* `./build-osx.sh --clean-all` will clean your work area of all assets.

* `./build-osx.sh --clean-and-package` will do a complete clean, then a complete build, then build
a Mac installer package which will be deposited in `products` folder.

* `./build-osx.sh --package` will create a `.pkg` file with a reasonable name. If you would like to use an
alternate version number, the packaging script is in `installer_mac` folder.

`./build-osx.sh` is also impacted by a couple of environment variables.

* `VST2SDK_DIR` points to a folder where VST2 SDK is located. If this is set, VST2 will build. If you set it after having
done a run without VST2, you will need to `./build-osx.sh --clean-all` to pick up the VST2 SDK path consistently.

### Using Xcode

If you would rather use Xcode directly, all of the install and build rules are exposed as targets.
You simply need to run CMake and open the Xcode project. From the command line:

```
cd surge
cmake . -GXcode -Bbuild
open build/Surge.xcodeproj
```

and you will see a set of targets like `install-au-local` which will compile and install the AU plugin.
These are the targets used by `build-osx.sh` from the command line.

## Linux

Most Linux systems have CMake and a modern C++ compiler installed. Make sure yours does.
You will also need to install a set of dependencies:

- build-essential
- libcairo-dev
- libxkbcommon-x11-dev
- libxkbcommon-dev
- libxcb-cursor-dev
- libxcb-keysyms1-dev
- libxcb-util-dev

For VST2, you will need the VST2 SDK - unzip it to a folder of your choice and set `VST2SDK_DIR` to point to it:

```
export VST2SDK_DIR="/your/path/to/VST2SDK"
```

Then fork this repository, `git clone` Surge and update the submodules:

```
git clone https://github.com/{your-user-name}/surge.git
cd surge
git submodule update --init --recursive
```

### Building with build-linux.sh

`build-linux.sh` is a wrapper on the various CMake and make commands needed to build Surge. As with
macOS, it is getting smaller every day as we move more things direclty into CMake.
As of Surge 1.7.1, `build-linux.sh` only works on x86 platforms. If you are building 1.7.1 on
ARM, please use the ARM specific instructions  <a href="#building-for-arm-platforms">below</a>. This is fixed at the head of the codebase, so
ARM builds can use `build-linux.sh` in the nightlies, or the upcoming 1.8 release.

You can build with the command:

```
./build-linux.sh build
```

or if you prefer a specific flavor:

```
./build-linux.sh build --project=lv2
```

which will run CMake and build the assets.

To use the VST2, VST3, or LV2, you need to install it locally along with supporting files. You can do this manually
if you desire, but the build script will also do it using the `install` option:

```
./build-linux.sh install --project=lv2 --local
```

Script will install VST2 to $HOME/.vst dir, VST3 to $HOME/.vst3 and LV2 to $HOME/lv2 in local mode.
To change this, edit vst2_dest_path and so forth to taste. Without --local, files will be installed to system locations (needs `sudo`).

For other options, you can do `./build-linux.sh --help`.

### Build using CMake directly

A build with CMake is also really simple:

```
cd surge
cmake . -Bbuild
cd build
make -j 2 Surge-VST3-Packaged
```

This will build the VST3 and deposit it in `surge/products` folder.

## Building for ARM platforms

As of August 4, `build-linux.sh` supports ARM builds. If you are building the 1.7.0 or
1.7.1 release, though, you need to follow these instructions.

With 1.7.0, we have merged changes needed to build with ARM platforms and have done some
Raspberry Pi testing. Due to a variety of choices an ARM user needs to make and due to
us not having a RPi in our pipeline (although we do a cross-compile test), we are not
building a binary of the ARM executable on Linux today, but you can build it easily.

You need to install the pre-requisites (`grep apt-get azure-pipelines.yml`)
and also install the packages `cairo-dev` and `libxcb-util0-dev`. Then
the steps to build using your native architecture on RPi are:

```
cmake -Bbuild -DARM_NATIVE=native
cmake --build build --config Release --target Surge-VST3-Packaged
```

The `-DARM_NATIVE=native` will include `cmake/arm-native.cmake`, which sets up native
CPU flags. If you want specific flags, copy that file to `cmake/arm-whatever.cmake`,
edit the flags, and use `-DARM_NATIVE=whatever`. If you set up an ARM build on a particular
architecture, we would appreciate a small CMake stub in a pull request from you!

Targets available are `Surge-VST3-Packaged`, `Surge-LV2-Packaged`, `Surge-VST2-Packaged` (if you have
the VST2 SDK) and `surge-headless`.

These commands will place your final product in `build/surge_products`. Since we have
not updated the `build-linux.sh` script for ARM yet, you need to do a couple of extra steps:

1. Copy the contents of `resources/data` to `/usr/share/Surge` or `~/.local/share/Surge`

2. Put your plugin in the appropriate location. For instance:

```
cd resources/data/
tar cf - . | ( cd ~/.local/share/Surge/; tar xf - )
cd ../../build/surge_products
mv Surge.vst3 ~/.vst3
```

3. There's a runtime requirement to install the free Lato font family, which is not a build
   requirement. `sudo apt-get install fonts-lato`

...and you should be good to go!

We welcome pull requests and contributions which would improve the ARM build experience!

## CMake Targets

If you are familiar with CMake, you can use it directly to build on any of our platforms,
and use it to install the build on Mac and Linux.

As normal, a CMake process begins by generating make assets:

| OS        | CMake Generation                            |
|-----------|---------------------------------------------|
| mac       | `cmake -Bbuild -GXcode`                     |
| win64     | `cmake -Bbuild`                             |
| win32     | `cmake -Bbuild -A Win32`                    |
| linux     | `cmake -Bbuild -DCMAKE_INSTALL_PREFIX=/usr` |
| linux-arm | `cmake -Bbuild -DARM_NATIVE=native`         |

At this point, you will have a `build` directory. You can now build targets using the command:

```
cmake --build build --config Release --target (target-name)
```

Available build targets are:

| Target              | Description                                                           |
|---------------------|-----------------------------------------------------------------------|
| Surge-VST3-Packaged | Produces the VST3 in `build/surge_products`                           |
| Surge-VST2-Packaged | Produces the VST2 in `build/surge_products` (only if VST2 is enabled) |
| Surge-AU-Packaged   | Produces the AU in `build/surge_products` (Mac only)                  |
| Surge-LV2-Packaged  | Produces the LV2 in `build/surge_products` (Linux only)               |
| surge-headless      | Builds the headless test component                                    |
| all-components      | Builds everything available on your OS                                |

On Mac and Linux, the CMake file also provides installation targets:

| Target                                    | Description                                                                                                       |
|-------------------------------------------|-------------------------------------------------------------------------------------------------------------------|
| install-everything-local                  | Install all components and resoures in the appropriate local location for your OS                                 |
| install-everything-global                 | Install all components and resources in the appropriate global location (driven by CMAKE_INSTALL_PREFIX on Linux) |
| install-resources-(global/local)          | Install just the resources locally or globally                                                                    |
| install-(vst2/vst3/au/lv2)-(global/local) | Install the plugin and associated resources locally or globally                                                   |

A reasonable session, then, could be (say, on Linux):

```
cmake -Bbuild -DCMAKE_INSTALL_PREFIX=/g/ins
cmake --build build --config Release --target all-components
sudo cmake --build build --config Release --target install-everything-global
```

which would result in the VST3 in `/g/ins/lib/vst3/Surge.vst3`, and so on.

# Continuous Integration

In addition to the build commands above, we use Azure pipelines for continuous integration.
This means that each and every pull request will be automatically built in all our environments,
and a clean build on all platforms is an obvious pre-requisite. If you have questions about
our CI tools, don't hesitate to ask on our [Discord](https://raw.githubusercontent.com/surge-synthesizer/surge-synthesizer.github.io/master/_includes/discord_invite_link)
server. We are grateful to Microsoft for providing Azure pipelines for free to the open source community!

# JUCE and SurgeEffectsBank

Since Nov 28, 2020, formerly separate SurgeEffectsBank plugin is a part of the main
Surge codebase, built with the master CMakeLists.txt file. SurgeEffectsBank bank uses JUCE and we
have adapted our environment to be able to build both JUCE and hand-crafted VST3SDK builds.

By default, JUCE builds are disabled for developers, but they are enabled in our CI and pipeline environments.

If you want to use the JUCE builds, you should add the `-DBUILD_SURGE_JUCE_PLUGINS=TRUE` command
to your first `cmake` command. This will download and activate JUCE and add additional targets
to your project. For instance, on Windows:

```
cmake -Bbuild -A x64 -DBUILD_SURGE_JUCE_PLUGINS=TRUE
```

will make a Surge solution file in `build` folder, which contains the standard Surge targets (like `Surge-VST3-Packaged`)
and also the SurgeEffectsBank targets (`Surge-Effects-Bank-Packaged`). You can then build using your platform favorite approach.
A portable approach is:

```
cmake --build build --config Release --target Surge-Effects-Bank-Packaged
```

Building this target will create `build/surge_products` directory which contains the platform specific targets. For instance,
on macOS it contains :

```
SurgeEffectsBank.app
SurgeEffectsBank.component
SurgeEffectsBank.vst3
```

The installer scripts generated by the pipeline will install the assets in the correct place, but we don't have CMake
install targets for SurgeEffectsBank yet. So for now you would need to move the built component to the appropriate location
on your system.

Note that JUCE creates VST3 specification adhering results, so on Windows and Linux, VST3 plugin is a directory which contains a DLL
in a path. That top level directory is your asset to be copied to `C:\Program Files\Common Files\VST3` on Windows, or `~/.vst3` on Linux.

# References

  * Most Surge-related conversation happens on the Surge Synthesizer Discord server. [You can join via this link](https://raw.githubusercontent.com/surge-synthesizer/surge-synthesizer.github.io/master/_includes/discord_invite_link)
  * IRC channel at #surgesynth at irc.freenode.net. The logs are available at https://freenode.logbot.info/surgesynth/.
  * Discussion at KvR forum [here](https://www.kvraudio.com/forum/viewtopic.php?f=1&t=511922)
