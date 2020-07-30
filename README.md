# Surge

Surge is an open source digital synthesizer, originally written and sold as a commercial product
by @kurasu/Claes Johanson at [vember audio](http://vemberaudio.se). In September of 2018,
Claes released a partially completed version of Surge 1.6 under GPL3, and a group
of developers have been improving it since. You can learn more about the team at https://surge-synth-team.org/

**If you are a musician only looking to use Surge please download the appropriate binary
[from our website](https://surge-synthesizer.github.io). The Surge developer team makes regular binary releases for all supported platforms
on the Surge website [https://surge-synthesizer.github.io](https://surge-synthesizer.github.io)**

If you would also like to participate in discussions, testing, and design of Surge, we have
details below and also in [the contributors section of the surge website](https://surge-synthesizer.github.io/#contributors).

Surge currently builds on macOS as a 64-bit AU, VST2 and VST3, Windows as a 64- and 32-bit VST2 and VST3
and Linux as a 64-bit VST2, VST3 and LV2. We provide binary distributions of the AU and VST3.

This README serves as the root of developer documentation for the project.

# Developing Surge

We welcome developers! Our workflow revolves around GitHub issues in this GitHub repository
and conversations in our Slack and IRC chatrooms. You can read our developer guidelines
in [our developer guide doc](doc/Developer%20Guide.md).

The developer guide also contains information about testing and debugging in particular hosts
on particular platforms.

We are starting writing a [guide to the internal architecture of Surge](doc/Architecture.md) which
can help you get oriented and answer some basic questions.

If you want to contribute and are new to git, we also have a [Git How To](doc/git-howto.md)
tailored at Surge development.

# Building Surge

As of April 2020, Surge is built using cmake. Versions in the 1.6 family require premake5 to build but that is
no longer required as of commit 6eaf2b2e20 or the 1.7 family. If you are generally familiar with and set up with cmake
you can use cmake directly to build targets such as "Surge.vst3" or "Surge.au".

## Windows

Additional pre-requisites:

* [Visual Studio 2017 (version 15.5 at least)](https://visualstudio.microsoft.com/downloads/)
* [Inno Setup](http://jrsoftware.org/isdl.php) for building the installer

### Install prerequisits

* Install Git, Visual Studio 2017 or newer
* If you want to build an installer, install Inno Setup
* When you install Visual Studio, make sure to include CLI tools and cmake, which are included in
'Optional CLI support' and 'Toolset for desktop' install bundles

### Check out the code

* Log into your GitHub account and fork this repository
* Open Visual Studio's command prompt. This is done by running `x64 Native Tools Command Prompt for VS 2017/2019` or similar which is installed along with Visual Studio. To do this, press Win key and start typing 'cmd', the command should pop up right away.
* In the Visual Studio command prompt, navigate to a writable directory you want to include Surge repository in
* In the Visual Studio command prompt, run these commands line by line to check out the code
```
git clone https://github.com/{your-user-name}/surge.git
cd surge
git submodule update --init --recursive
```

### Your first build

All of the following commands take place in Visual Studio command prompt.

* If you have the VST2 SDK and want to build the VST2 plugin, set the path to it as a user environment variable. If not, don't worry.
You can build the VST3 on Windows without any extra assets (we recommend all Windows users to
use the VST3). If you don't want to set up an environment variable, you can tell cmake the path to VST2 SDK like so:

```
set VST2SDK_DIR=c:\path\to\vst2
```

* Now, run cmake to create a build directory:

```
cmake . -Bbuild
```

* Choose to build in Visual Studio or the command line
   * To build in Visual Studio open the file `build/Surge.sln` created in the previous step. The internetIis full
     of introductions to help with Visual Studio once here.
   * To build from the command line, type:

```
cmake --build build --config Release --target Surge-VST3-Packaged
```

* After a succesful build, the folder `build/surge_products` will contain `Surge.vst3` which you can use to replace
  Surge in your DAW scan path. (Note: if you have never installed Surge you also need to install assets!)

### Your first 32-bit build

* 32-bit build is done exactly like 64-bit build, just with a couple of extra arguments. When you run cmake, add the `-A Win32` argument and choose a different target:

```
cmake . -Bbuild32 -A Win32
```

* To build the DLL, either open `build32\Surge.sln` or run the cmake build command with `build32` as the directory:

```
cmake --build build32 --config Release --target Surge-VST3-Packaged
```

### Building the Windows installer


If you want to build the installer, open the file `installer_win/surge.iss` with [Inno Setup](https://jrsoftware.org/isdl.php).
Inno will bake an installer and place it in `installer_win/Output/`


## macOS

To build on macOS, you need `Xcode`, `Xcode Command Line Utilities`, and cmake. Once you have installed
`Xcode` from the App Store, the command line to install the `Xcode Command Line Utilities` is:

```
xcode-select --install
```

There are a variety of ways to install cmake. If you run [homebrew](https://brew.sh) you can:

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
As of April 2020, it is a very thin wraper on cmake and Xcode.
It's what the primary Mac developers use day to day. The simplest approach is to build everything with:

```
./build-osx.sh
```

`build-osx.sh` will give you better output if you first `gem install xcpretty`, the Xcode build formatter,
and you have your `gem` environment running. If that doesn't work, don't worry - you can still build.

This command will build, but not install, the VST3 and AU components. It has a variety of options which
are documented in the `./build-osx.sh --help` screen,but a few key ones are:

* `./build-osx.sh --build-validate-au` will build the Audio Unit, correctly install it locally in `~/Library`
and run au-val on it. Running any of the installing options of `./build-osx` will install assets on your
system. If you are not comfortable removing an Audio Unit by hand and the like, please exercise caution!

* `./build-osx.sh --build-and-install` will build all assets and install them locally.

* `./build-osx.sh --clean-all` will clean your work area of all assets.

* `./build-osx.sh --clean-and-package` will do a complete clean, then a complete build, then build
a Mac installer package which will be deposited in `products`.

* `./build-osx.sh --package` will create a `.pkg` file with a reasonable name. If you would like to use an
alternate version number, the packaging script is in `installer_mac`.

`./build-osx.sh` is also impacted by a couple of environment variables.

* `VST2SDK_DIR` points to a VST2 SDK. If this is set, VST2 will build. If you set it after having
done a run without VST2, you will need to `./build-osx.sh --clean-all` to pick up VST2 path consistently.

### Using Xcode

If you would rather use Xcode directly, all of the install and build rules are exposed as targets.
You simply need to run cmake and open the Xcode project. From the command line:

```
cd surge
cmake . -GXcode -Bbuild
open build/Surge.xcodeproj
```

and you will set a set of targets like `install-au-local` which will compile and install the AU.
These are the targets used by `build-osx.sh` from the command line.

## Linux

Most linux systems have cmake and a modern C++ compiler installed. Make sure yours does.
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

`build-linux.sh` is a wrapper on the various cmake and make commands needed to build Surge. As with
macOS, it is getting smaller every day as we move more things direclty into cmake. You can now build with the command:

```
./build-linux.sh build
```

or if you prefer a specific flavor:

```
./build-linux.sh build --project=lv2
```

which will run cmake and build the assets.

To use the VST2, VST3, or LV2, you need to install it locally along with supporting files. You can do this manually
if you desire, but the build script will also do it using the `install` option:

```
./build-linux.sh install --project=lv2 --local
```

Script will install VST2 to $HOME/.vst dir, VST3 to $HOME/.vst3 and LV2 to $HOME/lv2 in local mode.
To change this, edit vst2_dest_path and so forth to taste. Without --local, files will be installed to system locations (needs sudo).

For other options, you can do `./build-linux.sh --help`.

### Build using cmake directly

A build with cmake is also really simple:

```
cd surge
cmake . -Bbuild
cd build
make -j 2 Surge.vst3
```

will build the VST3 and deposit it in surge/products.

## Building for ARM platforms

With 1.7.0 we have merged changes needed to build with ARM platforms and have done some 
raspberry pi testing. Due to a variety of choices an ARM user needs to make, and due to
us not having a Pi in our pipeline (although we do do a cross-compile test), we are not
building a binary of the ARM executable on Linux today, but you can build it easily.

You need to install the pre-requisites which are listed in azure-pipeline (`grep apt-get azure-pipelines.yml`)
and also install the packages `cairo-dev` and `libxcb-util0-dev `. Then 
the steps to build using your native architecture on the Pi are:

```
cmake -Bbuild -DARM_NATIVE=native
cmake --build build --config Release --target Surge-VST3-Packaged
```

The `-DARM_NATIVE=native` will include `cmake/arm-native.cmake` which sets up the native
CPU flags. If you want specific flags, copy that file to `cmake/arm-whatever.cmake`,
edit the flags, and use `-DARM_NATIVE=whatever`. If you set up an ARM build on a particular
architecture we would appreciate you sharing the small cmake stub in a pull request.

Targets available are `Surge-VST3-Packaged`, `Surge-LV2-Packaged`, `Surge-VST2-Packaged` (if you have
the VST2 SDK) and `surge-headless`.

These commands will place your final product in `build/surge_products`. Since we have 
not updated the build-linux script for ARM yet you need to do a couple of extra steps:

1. Copy the contents of `resources/data` to `/usr/share/Surge` or `~/.local/share/Surge`.
   Put your plugin in the appropriate location.
   For instance

```
cd resources/data/
tar cf - . | ( cd ~/.local/share/Surge/; tar xf - )
cd ../../build/surge_products
mv Surge.vst3 ~/.vst3
```

2. There's a runtime requirement to install the free Lato font family which is not a build
   requirement. `sudo apt-get install fonts-lato`

And you should be good to go.

We welcome PRs and contributions which improve the ARM build experience.

# Continuous Integration

In addition to the build commands above, we use Azure pipelines to do continuous integration.
This means each of your pull requests will be automatically built in all of our environments,
and a clean build on all platforms is an obvious pre-requisite. If you have questions about
our CI tools, please ask on our Slack channel. We are grateful to Microsoft for providing
Azure pipelines for free to the open source community!

# References

  * Most Surge-related conversation on the Surge Synth Slack. [You can join via this link](https://raw.githubusercontent.com/surge-synthesizer/surge-synthesizer.github.io/master/_includes/slack_invite_link)
  * IRC channel at #surgesynth at irc.freenode.net. The logs are available at https://freenode.logbot.info/surgesynth/.
  * Discussion at KvR forum [here](https://www.kvraudio.com/forum/viewtopic.php?f=1&t=511922)
  * Chain icon on filter link from flaticon.com
