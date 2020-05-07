# Surge

Surge is an Open Source Digital Synthesizer, originally written and sold as a commercial product
by @kurasu/Claes Johanson at [vember audio](http://vemberaudio.se). In September of 2018,
Claes released a partially completed version of Surge 1.6 under GPL3, and a group
of developers have been improving it since. You can learn more about the team at https://surge-synth-team.org/

**If you are a musician only looking to use Surge please download the appropriate binary
[from our website](https://surge-synthesizer.github.io). The Surge developer team makes regular binary releases for all supported platforms 
on the Surge website [https://surge-synthesizer.github.io](https://surge-synthesizer.github.io)**

If you would also like to participate in discussions, testing, and design of Surge, we have
details below and also in [the contributors section of the surge website](https://surge-synthesizer.github.io/#contributors).

Surge currently builds on macOS as a 64 bit AU, VST2 and VST3, Windows as a 64 and 32 bit VST2 and VST3 
and Linux as a 64 bit VST2, VST3 and LV2. 

This README serves as the root of developer documentation for the project.

# Developing Surge

We welcome developers. Our workflow revolves around github issues in this github repo
and conversations in our slack and IRC chatrooms. You can read our developer guidelines
in [our developer guide doc](doc/Developer%20Guide.md).

The developer guide also contains information about testing and debugging in particular hosts
on particular platforms.

We are starting writing a [guide to the internal architecture of Surge](doc/Architecture.md) which
can help you get oriented and answer some basic questions.

If you want to contribute and are new to git, we also have a [Git HowTo](doc/git-howto.md)
tailored at Surge dev.

# Building Surge

As of April 2020, Surge is built using cmake. Versions in the 1.6 family require premake5 to build but that is
no longer required as of commit 6eaf2b2e20 or the 1.7 family. If you are generally familiar with and set up with cmake
you can use cmake directly to build targets such as "Surge.vst3" or "Surge.au".

## Windows

Additional pre-requisites:

* [Visual Studio 15.5 (at least)](https://visualstudio.microsoft.com/downloads/)
* [Inno Setup](http://jrsoftware.org/isdl.php) for building the installer

Install Git, Visual Studio 2017 or 2019, and (if you want to build an installer) Inno Setup. When you 
install Visual Studio, make sure to include the CLI tools and cmake, which are included in
'optional CLI support' and 'toolset for desktop' install bundles.

You then want to start a visual studio prompt. This is a command like `x64 Native Tools Command Prompt for VS 2017` or similar installed by your visual studio install.

After all this is done, make a fork of this repo, clone the repo and get the required 
submodules with the following commands in that command shell.

```
git clone https://github.com/{your-user-name}/surge.git
cd surge
git submodule update --init --recursive
```

If you have the VST2SDK and are building the VST2, set your environment. If not, don't worry.
You can build the VST3 on windows without any extra assets (we recommend all windows users
use the VST3).

```
set VST2SDK_DIR=c:\path\to\vst2
```

Next, run cmake to construct the appropriate build files

```
cmake . -Bbuild
```

After which you can open the freshly generated Visual Studio solution `build/Surge.sln`. You can also
compile from the command line with

```
msbuild buildtask.xml /p:Platform=x64
```

If you want to build the installer, open the file `installer_win/surge.iss` by using `Inno Setup`. 
`Inno Setup` will bake an installer and place it in `installer_win/Output/`


## macOS

To build on macOS, you need `Xcode`, `Xcode Command Line Utilities`, and cmake. Once you have installed
`Xcode` from the mac app store, the commandline to install the `Xcode Command Line Utilities` is 

```
xcode-select --install
```

There are a variety of ways to instal cmake. If you run [homebrew](https://brew.sh) you can

```
brew install cmake
```

Once that's done, fork this repo, clone it, and update submodules.

```
git clone https://github.com/{your-user-name}/surge.git
cd surge
git submodule update --init --recursive
```

### Building with build-osx.sh

`build-osx.sh` has all the commands you need to build, test, locally install, validate, and package Surge on Mac.
As of April 2020, it is a very thin wraper on cmake and xcode. 
It's what the primary Mac developers use day to day. The simplest approach is to build everything with

```
./build-osx.sh
```

`build-osx.sh` will give you better output if you first `gem install xcpretty`, the xcodebuild formatter, 
and you have your gem environment running. If that doesn't work don't worry - you can still build.

this command will build, but not install, the VST3 and AU components. It has a variety of options which
are documented in the `./build-osx.sh --help` screen but a few key ones are:

* `./build-osx.sh --build-validate-au` will build the audio unit, correctly install it locally in `~/Library`
and run au-val on it. Running any of the installing options of `./build-osx` will install assets on your
system. If you are not comfortable removing an audio unit by hand and the like, please exercise caution.

* `./build-osx.sh --build-and-install` will build all assets and install them locally

* `./build-osx.sh --clean-all` will clean your work area of all assets

* `./build-osx.sh --clean-and-package` will do a complete clean, thena complete build, then build
a mac installer package which will be deposited in products.

* `./build-osx.sh --package` will create a `.pkg` file with a reasonable name. If you would like to use an 
alternate version number, the packaging script is in `installer_mac`.

`./build-osx.sh` is also impacted by a couple of environment variables.

* `VST2SDK_DIR` points to a vst2sdk. If this is set vst2 will build. If you set it after having
done a run without vst2, you will need to `./build-osx.sh --clean-all` to pick up vst2 consistently
 
### Using Xcode

If you would rather use xcode directly, all of the install and build rules are exposed as targets.
You simply need to run cmake and open the xcode project. From the command line:

```
cd surge
cmake . -GXcode -Bbuild
open build/Surge.xcodeproj
```

and you will set a set of targets like `install-au-local` which will compile and install the au. 
These are the targets used by `build-osx.sh` from the command line.

## Linux

Most linux systems have cmake and a modern C++ compiler installed. Make sure yours does.
You will also need to install a set of dependencies.

- build-essential
- libcairo-dev
- libxkbcommon-x11-dev
- libxkbcommon-dev
- libxcb-cursor-dev
- libxcb-keysyms1-dev
- libxcb-util-dev

For VST2, you will need the `VST2 SDK` - unzip it to a folder of your choice and set `VST2SDK_DIR` to point to it.

```
export VST2SDK_DIR="/your/path/to/VST2SDK"
```


Then fork this repo, `git clone` surge and update the submodules:

```
git clone https://github.com/{your-user-name}/surge.git
cd surge
git submodule update --init --recursive
```

### Building with build-linux.sh

`build-linux.sh` is a wrapper on the various cmake and make commands needed to build Surge. As with
macOS, it is getting smaller every day as we move more things direclty into cmake. You can now build with the command

```
./build-linux.sh build
```

or if you prefer a specific flavor


```
./build-linux.sh build --project=lv2
```

which will run cmake and build the assets.

To use the VST2, VST3, or LV2, you need to install it locally along with supporting files. You can do this manually
if you desire, but the build script will also do it using the `install` option.

```
./build-linux.sh install --project=lv2 --local 
```

Script will install vst2 to $HOME/.vst dir, vst3 to $HOME/.vst3 and LV2 to $HOME/lv2 in local mode. 
To change this, edit vst2_dest_path and so forth to taste. Without --local files will be installed to system locations (needs sudo).

For other options, you can do `./build-linux.sh --help`.

### Build using cmake directly

A build with cmake is also really simple. 

```
cd surge
cmake . -Bbuild
cd build
make -j 2 Surge.vst3
```

will build the VST3 and deposit it in surge/products.

# Continuous Integration

In addition to the build commands above, we use azure pipelines to do continuous integration.
This means each of your pull requests will be automatically built in all of our environments,
and a clean build on all platforms is an obvious pre-requisite. If you have questions about 
our CI tools, please ask on our Slack channel. We are grateful to Microsoft for providing 
azure pipelines to free to the open source community!

# References

  * Most Surge-related conversation on the Surge Synth Slack. [You can join via this link](https://raw.githubusercontent.com/surge-synthesizer/surge-synthesizer.github.io/master/_includes/slack_invite_link)
  * IRC channel at #surgesynth at irc.freenode.net. The logs are available at https://freenode.logbot.info/surgesynth/.
  * Discussion at KVR-Forum [here](https://www.kvraudio.com/forum/viewtopic.php?f=1&t=511922)
