# Developing with our VSTGUI Fork

## Motivation 

We made the hard choice [here](https://github.com/surge-synthesizer/surge/issues/515) to fork
vstgui so we can maintain our own fixes while also submitting them to steinberg. That issue describes
a lot of the conversation, but our key motivation was

* We wish to track the master version of vstgui closely
* We need to fix bugs in vstgui which expose Surge to odd behaviors
* We are happy to fix those bugs and send those changes to the vstgui team, but would like to ship Surge with
the fixes without having to wait for the vstgui team to review and merge them, since their current cadence is different
than our current cadence.

So we decided to take make our own fork of vstgui, have a branch of our own which is branched
from master, add patches to our branch, and submit those patches as PRs to steinberg.
On some regular basis we will rebase our branch to the latest steinberg master.

## How it is set up

* [`surge-synthesizer/vstgui`](https://github.com/surge-synthesizer/vstgui) is a fork 
of [`steinbergmedia/vstgui`](https://github.com/steinbergmedia/vstgui) with default branch set to master
* [`surge-synthesizer/vstgui:surge`](https://github.com/surge-synthesizer/vstgui/tree/surge) is a branch 
from [`surge-synthsizer/vstgui:master`](https://github.com/surge-synthesizer/vstgui/tree/master) with changes we have applied along the way
* [`surge-synthesizer/surge`](https://github.com/surge-synthesizer/surge) has a submodule `vstgui.surge` which points to the `surge` branch of our `surge-synthesizer/vstgui` fork.
* `surge-synthesizer/surge` premake uses the version of vstgui from the submodule `vstgui.surge` rather than the version from the submodule `vst3sdk/vstgui4/vstgui`

This amounts to us having a repo which is ahead of `steinbergmedia/vstgui:master` while still having the master version of the rest of
the `vst3sdk` components.

Please note that `steinbergmedia/vstgui` has `develop` as its default branch. This means branches like our `surge` branch (which is a branch from `master`) and 
steinberg's `master` branch will 
report being many commits behind the repo in the github UI. We think it is prudent for surge to track a patcked master, not a patched develop, so have changed our default
branch in our fork back to master.

## How to develop

There's a couple of ways for you to develop changes but the basic idea is as follows.
This is a bit tricky. If you are doing this for the first time, probably best to ask
on slack or IRC.

* Fork your own copy of https://github.com/surge-synthesizer/vstgui or work directly in the repo. *Do not fork the 
steinberg repository. You want a fork of our repository (so a fork-of-a-fork)*.
* Clone either the repo or your fork on your filesystem and set the environment variable `VSTGUI_DIR` to point at it. Clean and run premake in `surge`.
* Checkout the surge branch in your vstgui
* From there, make a feature branch. So 'git checkout -b my-change' on top of 'surge' not 'master'
* Make your change, commit, push as normal. Since VSTGUI_DIR points to your copy your builds will pick up your changes
* At this point we want to do two things
  * Take your branch as a PR to steinberg. Do this using standard methods.
  * Integrate your branch into the surge repository and move the submodule pointer
  * That last step is a bit tricky. Really best to ask on Slack or IRC if you are
    doing this for the first time. Or just let one of the maintainers know and we
    can do the git shenanigans.

This documentation could be more fulsome obviously. Really the first person other than @baconpaul to
go through the process, if they could document it, we can update this doc then.


## Historical Record: How I set up the thing in the first place

* fork steinbergmedia/vstgui into surge-synthesizer
* set the default branch as "master"
* git checkout -b surge
* git push origin surge
* modify premake to set up a vstguidir

To apply my first diff

* get a vstgui
* git checkout surge
* git checkout -b surge-menu
* fix
* git commit
* git checkout surge
* git merge surge-menu
* git push

Now add this as a submodule

* git submodule add -b surge --name vstgui.surge https://github.com/surge-synthesizer/vstgui vstgui.surge

And write this doc and we are done


