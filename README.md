# YOU ARE LOOKING AT THE EXPERIMENTAL BRANCH

# Surge

Surge is an Open Source Digital Synthesizer, originally written and sold as a commercial product
by @kurasu/Claes Johanson at [vember audio](http://vemberaudio.se). In September of 2018,
Claes released a partially completed version of Surge 1.6 under GPL3, and a group
of developers have been improving it since.

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

To build surge, you need  [Git](https://git-scm.com/downloads) and [Premake 5](https://premake.github.io/download.html#v5) 
installed with the appropriate version for your system. We are migrating towards [CMake](https://cmake.org) so for
some OSes you need to install CMake also.

## Windows

Additional pre-requisites:

* [Visual Studio 15.5 (at least)](https://visualstudio.microsoft.com/downloads/)
* [Inno Setup](http://jrsoftware.org/isdl.php) for building the installer

Install Git, Premake5, Visual Studio 2017 and (if you want to build an installer) Inno Setup. 

If you are doing a fresh install of Visual Studio Community Edition, after you install run Visual Studio installer, 
update, and then make sure the `desktop C++ kit`, including `optional CLI support`, `Windows 8.1 SDK`, 
and `VC2015 toolset for desktop` are installed. 

After all this is done, make a fork of this repo, clone the repo and get the required 
submodules with the following commands.

```
git clone https://github.com/{your-user-name}/surge.git
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

After which you can open the freshly generated Visual Studio solution `Surge.sln` - If you had the `VST2.4SDK` folder specified 
prior to running `premake5`, you will have `surge-vst2.vcxproj` and `surge-vst3.vcxproj` in your Surge folder.

Now, to build the installer, open the file `installer_win/surge.iss` by using `Inno Setup`. `Inno Setup` will bake an installer and place it in `installer_win/Output/`


## macOS

In addition to having Git and premake5 in your path, Surge builds require you have both `Xcode` and `Xcode Command Line Utilities` installed. 
The commandline to install the `Xcode Command Line Utilities` is 

```
xcode-select --install
```

Once that's done, fork this repo, clone it, and update submodules.

```
git clone https://github.com/{your-user-name}/surge.git
cd surge
git submodule update --init --recursive
```

### Building with build-osx.sh

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

* `./build-osx.sh --package` will create a `.pkg` file with a reasonable name. If you would like to use an 
alternate version number, the packaging script is in `installer_mac`.

`./build-osx.sh` is also impacted by a couple of environment variables.

* `VST2SDK_DIR` points to a vst2sdk. If this is set vst2 will build. If you set it after having
done a run without vst2, you will need to `./build-osx.sh --clean-all` to pick up vst2 consistently

* `BREWBUILD=TRUE` will use homebrew clang. If XCode refuses to build immediately with `error: invalid value 'c++17' in '-std=c++17'` 
and you do not want to upgrade XCode to a more recent version, use [homebrew](https://brew.sh/) to install llvm and set this variable.
 
### Using Xcode

`premake xcode4` builds xcode assets so if you would rather just use xcode, you can open the solutions created after running premake yourself or having `./build-osx.sh` run it.

After the build runs, be it successful or not, you can now launch Xcode and open the `Surge` folder. Let Xcode do it's own indexing / processing, which will take a while.

All of the three projects (`surge-vst3`, `surge-vst2`, `surge-au`) will recommend you to `Validate Project Settings`, meaning, more precisely, to `Update to recommended settings`. By clicking on `Update to recommended settings`, a dialog will open and you'll be prompted to `Perform Changes`. Perform the changes.

Xcode will result in built assets in `products/` but will not install them and will not install the ancilliary assets. To do that you can either `./build-osx.sh --install-local` or `./build-osx.sh --package` and run the resulting pkg file to install in `/Library`.


## Linux

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

Then fork this repo, `git clone` surge and update the submodules:

```
git clone https://github.com/{your-user-name}/surge.git
cd surge
git submodule update --init --recursive
```

You can now build with the command

```
./build-linux.sh build
```

or if you prefer a specific flavor


```
./build-linux.sh build --project=lv2
```

which will run premake and build the assets.

To use the VST2, VST3, or LV2, you need to install it locally along with supporting files. You can do this manually
if you desire, but the build script will also do it using the `install` option.

```
./build-linux.sh install --project=lv2 --local 
```

Script will install vst2 to $HOME/.vst dir, vst3 to $HOME/.vst3 and LV2 to $HOME/lv2 in local mode. To change this, edit vst2_dest_path and so forth to taste. Without --local files will be installed to system locations (needs sudo).

For other options, you can do `./build-linux.sh --help`.

# Continuous Integration

In addition to the build commands above, we use azure pipelines to do continuous integration.
This means each of your pull requests will be automatically built in all of our environments,
and a clean build on all platforms is an obvious pre-requisite. If you have questions about 
our CI tools, please ask on our Slack channel.

# References

  * Most Surge-related conversation on the Surge Synth Slack. [You can join via this link](https://join.slack.com/t/surgesynth/shared_invite/enQtNTE4OTg0MTU2NDY5LTE4MmNjOTBhMjU5ZjEwNGU5MjExODNhZGM0YjQxM2JiYTI5NDE5NGZkZjYxZTkzODdiNTM0ODc1ZmNhYzQ3NTU)
  * IRC channel at #surgesynth at irc.freenode.net. The logs are available at https://freenode.logbot.info/surgesynth/.
  * Discussion at KVR-Forum [here](https://www.kvraudio.com/forum/viewtopic.php?f=1&t=511922)
