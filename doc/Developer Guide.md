# Developer Guide

## Introduction

This document contains the guidelines that we use while developing Surge. Please
follow them whenever possible. If you have further questions, look at README.md
for an up to date list of Surge communication channels.

## Contributing

### Code reviews

Code reviews are important parts of pull requests. Folks do them carefully and
seriously. But we are always polite and professional. If you think a comment about
your code is somehow rude or impolite, please don't; nobody in the Surge community has that
intent.

But please, do expect your code to be reviewed before it is merged, and you may be asked
to make changes. Skewing to these rules and checking them before you do a PR can help
make everyone's time more efficient.

### Commit messages

Format:

```
<short summary one-line summary>

<long description describing the change in detail>

<optional tag>
```

A tag can be either `Related <issue number>`, `Closes <issue number>` or `Addresses <issue number>`.


### GitHub usage

* The [git-howto](git-howto.md) is a good basic primer if you are new to `git`.
* Generally, try and have a GitHub issue which documents what you are trying to do
* Name your branch with a descriptive name and an issue number. For instance,
`mac-wav-file-198` is a branch to implement WAV file reading on Mac, in response to issue #198
* Try to have pull requests with one or a small number of commits. If your change has two
logical big steps, two commits is fine, but work commits where you just make a small change
could be squashed with `git rebase`. If you don't know how to do this, ask us on Surge Discord server, or
put in the PR with your commits and we can squash at merge
* Try to avoid trivial or unrelated diffs. Your diffs should be "about" the same size as your change.

## Coding style

### Basics

* Obey the 'campground rule' that the code you change should be better. Cleaner, better names, more comments, etc.
* At the same time, obey the 'small change' rule, which is: most of the time the smallest
change is the best change. So, editing a function with a variable name you don't like is OK
if that variable name is in 30 other places.
* Use spaces, not tabs
* The code is formatted with 4 spaces tab width, and we use `clang-format` to automatically style things as you type.
Any and all pull requests are verified against `clang-format` and if a PR doesn't adhere to the formatting rules we've set,
CI will fail (`linux-codequality` job). Make sure the diffs you submit for merging are compliant to our `clang-format`!

### Naming

* `#define` constants are `UPPERCASE_VARIABLES`
* `class HaveCamelCaseNames`
* `void functionsAreCamelCaseWithLowerFirst`
* We are not using `s_` or `m_` or equivalent notations for members or statics
* Full namespaces are generally prefered over `using`. We are trying to use
`std::vector` over `vector` in the code.
* Don't do `using namespace` in header files. Don't `using namespace std` in new code (but it is in some existing code).
* Use namespaces, not classes to group functions. Check out how we implemented `UserInteractions.h`
* Long and descriptive names are good. `userMessageDeliveryPipe` is better than `umdp`.

### Comments

Comments for declarations of member functions or free functions aspires to javadoc/doxygen style.

```
class Example
{
    /**
     * replace the printer
     * @param printerName which printer to replace
     *
     * Replace the printer
     */
    void replacePrinter(std::string printerName);
}
```

Comments for bigger chunks of code inline use a multiline comment

```
   ...
   x = 3 + y;

   /*
   ** Now we have to implement the nasty search
   ** across the entire set of the maps of sets
   ** and there is no natural index, so use this
   ** full loop
   */
   for (auto q ...
```

Use single line comments sparingly, but where appropriate, feel free.

Generally: Comment your code. Someone coming after you will thank you. And that someone may be you!

### Format your code properly with clang-format

Surge ships with a `.clang-format` file which makes choices about how to format code. You can
use this to make sure your diffs are formatted properly. Here's how:

* install `clang-format` and `git-clang-format`. On macOS this can be done with
  `brew install clang-format`. On Linux and Windows it is left as an exercise to the reader.
  If you complete that exercise, please update this doc!
* Develop and so on as you normally would following the prescription in our [git howto](git-howto.md)
* Format the code at one of two development points:
  * Before you commit, but after you add, do a `git clang-format` and it will correct your code
  * Add a series of commits on your branch (because you are on branch of course to develop)
    then at the very end, `git clang-format main` will reformat all your changes from main.
    Commit and squash that commit.


### Miscellaneous

Occasionally, code needs an `#if MAC` but if you have entire classes with parallel implementations,
just put the implementations in the `src/mac` `src/windows` and `src/linux` directories and
let CMake pick the right one. This does mean that you need stubs on all three platforms to link.
Look at the `UserInteractions.h` example.

The only numbers which make sense in code are 0, 1, n, and infinity. If you are using a number other
than that, perhaps toss in a comment.

Prefer `std::ostringstream` and so on to `sprintf` and so on.

`#pragma once`, while not [really standard](https://en.wikipedia.org/wiki/Pragma_once), is used in
most of the code, so we are continuing to use it rather than `#ifdef` guards.

## Tips for developing on particular platforms

First and foremost: Do what works for you. These are just some collected tips from those
of us who have been working on Surge. They may or may not work for you. If you have good
ideas to help developers, please add them here!

### Mac

The easiest way to debug on Mac that `@baconpaul` found is to make it so you
can run the plugin in a host and see `stdout`.

To use the Audio Unit plugin in Logic, Mainstage, GarageBand etc., the very first time you need to
do a one-time step which is to invalidate your AU cache so that you force Logic (and others) to rescan plugins.
The easiest way to do this is to move the AudioUnitCache away from its location by typing in:

```
mv ~/Library/Caches/AudioUnitCache ~/Desktop
```

As for AU host, the easiest way to do this is with [Hosting AU](http://ju-x.com/hostingau.html).
Install it and set up a single track containing a Surge instance. Save that Hosting AU
configuration as (say) `~/Desktop/Surge.hosting`, then run:

```
/Applications/Hosting\ AU.app/Contents/MacOS/Hosting\ AU ~/Desktop/Surge.hosting
```

You can route MIDI to that, play it with the keyboard using Hosting AU keyboard driver, and so on.

You can similarly capture `stdout` from Logic (or other hosts) by running the appropriate command
for the inside of the app from Terminal. For instance: `/Applications/Logic\ Pro\ X.app/Contents/MacOS/Logic\ Pro\ X ` will run
Logic and show you `stdout`.

For VST2, [Carla](https://kxstudio.linuxaudio.org/Applications:Carla) provides a super lightweight VST2 wraparound.
Just unpack it into `~/Desktop/Carla`. You also need to install JACK, so:

```
brew install jack
jackd -d coreaudio
```

and then after a build you can load the VST2 plugin into a single host with:

```
~/Desktop/Carla/carla.lv2/carla-bridge-native vst2 products/Surge.vst/ 'none' '(0)'
```

Finally, Bitwig Studio is an excellent host for development since you don't have to restart
it between builds. `stdout` goes to the logs in `~/Library/Logs/Bitwig`

Profiling on macOS is awesome too! Start the synth doing something, then:

```
instruments -l 30000 -t Time\ Profiler -p pid
```

and look at the output with Open.

### Linux

You can get a very clean debugging environment using Carla and Surge where you can
play notes using an on-screen keyboard and route them to the VST2 with `stdout` visible.
Everything here is documented for Ubuntu 18.04 LTS. Different packages have different
things of course.

To do that, you first need to install a bunch of JACK code:

```
sudo apt-get install jack
sudo apt-get install jack-keyboard
sudo apt-get install ajmidid
```

Then run `jac` and `ajmidid` in separate terminals:

```
jackd -d alsa -X alsa_midi
a2jmidid -e
```

Now start Surge with `carla-single`:

```
~/Carla_2.0-RC3-linux64/carla/carla-bridge-native vst2 ./target/vst2/Release/Surge.so  '(none)' '0'
```

and then in a fourth keyboard, start `jack-keyboard`. The Connect To option should have
Surge as an endpoint. Pick it and you can play the GUI keyboard and hear and watch the
`stdout` go by.

To run `valgrind`:

```
./build-linux.sh build --project=headless
valgrind --leak-check=yes build/surge-headless
```

and remember:

```
#include <valgrind/memcheck.h>
#include <iostream>

...

  int notDef;
  if( auto v = VALGRIND_CHECK_MEM_IS_DEFINED(notDef) )
    std::cout << "Memory notDef is not defined at " << v << std::cout
```
