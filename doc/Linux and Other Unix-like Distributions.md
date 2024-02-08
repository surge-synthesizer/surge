## About the Dependencies

The list of dependencies you'll find in the [README](https://github.com/surge-synthesizer/surge/blob/main/README.md#linux), is based on Ubuntu 18.04 and 20.04 LTS. <br/>
The exact package names may be different for other distros. <br/>
The compiler may notify you that it cannot find `libcurl`, `webkit2gtk-4.0` and `gtk+-x11-3.0` <br/>
should you not have it installed. These are JUCE dependencies Surge itself does not use. <br/>
You can install them if you want to silence the message but Surge will build fine without. <br/>
Likewise JACK is only mandatory if you want to build the standalone version of Surge.

## Binary Installers

The Surge team makes Linux .deb and .rpm files available for the surge distribution at <br/>
https://surge-synthesizer.github.io. <br/>
These are built on an Ubuntu 18.04 VM in our azure pipeline from source, packaged as .deb and .rpm files <br/>
using the `surge-xt-distribution` target (src/cmake/lib.cmake) and made available as binaries. <br/>
You can download the .deb file for example and install it with `dpkg` as you would any other binary distribution.

## Platforms

Surge is buildable from source on a wide variety of platforms.<br/>
If you would like to build a version consistent with a release, each of our release points are available <br/> 
in a `release/version` branch (for instance `release/1.6.1.1`) in our github repo; <br/>
and the nightly .deb is built from the head of main on each push to main.

## Packages and Ports

The following lists a couple of packages and ports maintained by other projects, <br/>
stored in appropriate distro repos away from this core Surge distribution. <br/>
If you encounter problems with these ports please contact the maintainer of the package. <br/><br/>
We think the effort that is undertaken to make Surge available everywhere is amazing <br/>
and we try to keep track of it all but if you see anything outdated here <br/>
please send a pull-request updating the info.

## Surge in KXStudio

[@falkTX](https://github.com/falkTX/) packages up Surge as one of the synthesizers in KXStudio.  <br/>
You can find out all about the KX distribution at https://kx.studio

KX builds install with a version change to make sure KX overwrites other installs.  <br/>
This means if you install 1.9 from a .deb file, KX will overwrite it. To avoid this,  <br/>
pin the `surge-synthesizer` and `surge-data` packages as discussed [here](https://github.com/KXStudio/KXStudio/issues/14).

1. Create a file in the directory `/etc/apt/preferences.d`  <br/>
named `surge-synthesizer` with the following content:
```bash
#
# apt pinning for installing Surge 1.9 over kxstudio
#
Package: surge
Pin: version 1.9.0
Pin-Priority: 1001
```
2. Save it and do `sudo apt update && sudo apt upgrade`
3. Surge 1.9 will pin over KX
  
- **Please note:** *For **Surge XT** the pinning is currently not necessary.*

## Surge on FreeBSD

The FreeBSD port is available and maintained at: https://www.freshports.org/audio/surge-synthesizer-xt-lv2/

Users can install it with the command: <br/>
`pkg install surge-synthesizer-xt-lv2` (using a binary package) <br/>
`cd /usr/ports/audio/surge-synthesizer-xt-lv2 && make install clean`
(build and install from source)

## Surge on Arch Linux

The Arch Linux port is [available](https://archlinux.org/packages/community/x86_64/surge-xt/) and maintained in the official repositories.

Users can install it with the command: <br/>
`pacman -Syu surge-xt` <br/>

If there are any issues specific to this port, please e-mail the maintainers (contact details found in above link)
or file a bug report on the [bug tracker](https://bugs.archlinux.org/).

## Surge on Flathub

The latest addition among the packages is here: <br/>
https://flathub.org/apps/details/org.surge_synth_team.surge-xt <br/>
- The plugins _for use with other flatpaked apps_ is available as `org.freedesktop.LinuxAudio.Plugins.Surge-XT`
