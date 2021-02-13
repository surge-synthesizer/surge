# Developing with our VSTGUI Fork

## Motivation 

We made the hard choice [here](https://github.com/surge-synthesizer/surge/issues/515) to fork
VSTGUI so we can maintain our own fixes, while also submitting them to Steinberg. That issue describes
a lot of the conversation, but our key motivation was:

* We wish to track the master version of VSTGUI closely
* We need to fix bugs in VSTGUI which expose Surge to odd behaviors
* We are happy to fix those bugs and send those changes to the VSTGUI team, but would like to ship Surge with
the fixes without having to wait for the VSTGUI team to review and merge them, since their current cadence is different
than our current cadence.

So we decided to take make our own fork of VSTGUI, have a branch of our own which is branched
from master, add patches to our branch, and submit those patches as PRs to Steinberg.
On some regular basis we will rebase our branch to the latest Steinberg master.

## How it is set up

* [`surge-synthesizer/vstgui`](https://github.com/surge-synthesizer/vstgui) is a fork 
of [`steinbergmedia/vstgui`](https://github.com/steinbergmedia/vstgui) with default branch set to master
* [`surge-synthesizer/vstgui:surge`](https://github.com/surge-synthesizer/vstgui/tree/surge) is a branch 
from [`surge-synthsizer/vstgui:master`](https://github.com/surge-synthesizer/vstgui/tree/master) with changes we have applied along the way
* [`surge-synthesizer/surge`](https://github.com/surge-synthesizer/surge) has a submodule `vstgui.surge` which points to the `surge` branch of our `surge-synthesizer/vstgui` fork.
* `surge-synthesizer/surge` make system uses the version of vstgui from the submodule `vstgui.surge` rather than the version from the submodule `vst3sdk/vstgui4/vstgui`

This amounts to us having a repo which is ahead of `steinbergmedia/vstgui:master`, while still having the master version of the rest of
the `vst3sdk` components.

Please note that `steinbergmedia/vstgui` has `develop` as its default branch. This means branches like our `surge` branch (which is a branch from `master`) and 
steinberg's `master` branch will 
report being many commits behind the repo in the GitHub UI. We think it is prudent for Surge to track a patched master, not a patched develop, so have changed our default
branch in our fork back to master.

## How to develop

There's a couple of ways for you to develop changes, but the basic idea is as follows.
This is a bit tricky. If you are doing this for the first time, probably best to ask us
on Discord.

* Has Steinberg fixed this in `develop`? Check first! If so, pull that diff. If this doesn't make sense, ask @baconpaul on Discord for now.
* Fork your own copy of https://github.com/surge-synthesizer/vstgui or work directly in the repo. *Do not fork the 
Steinberg repository. You want a fork of our repository (so a fork-of-a-fork)*.
* Clone either the repo or your fork on your filesystem and set the environment variable `VSTGUI_DIR` to point at it. Clean and run cmake in `surge`.
* Checkout the Surge branch in your VSTGUI
* From there, make a feature branch. So 'git checkout -b my-change' on top of 'surge' not 'master'
* Make your change, commit, push as normal. Since VSTGUI_DIR points to your copy your builds will pick up your changes.
  * Please note that VSTGUI uses a different code formatting convention than Surge
  * VSTGUI uses tabs, not spaces; uses space after keywords and before parentheses
  * there is a .clang-format in the top dir
  
Now you have two more steps. Uhh, really, if you are doing this ask @baconpaul, OK? This is not user self-service level documentation.

### Move the Surge pointer to include your change

* This is something the maintainers will do. Basically just push your branch to someplace where the maintainers can see it.
* Mainters: Do this
  * in `vstgui`
    * `git checkout surge`
    * `git merge other-persons-branch` and resolve anything that comes up
    * `git cherry -v master` - make sure all the commits between master and surge make sense
    * `git push origin surge` - now surge is up on surge-synthesizer
  * in `surge`
    * add a feature branch 
    * `cd vstgui.surge`
    * `git fetch origin`
    * `git checkout surge`
    * `git pull surge`
    * `cd ..`
    * `git status` should show vstgui.surge as modified so
    * `git add vstgui.surge`
    * then commit and send PR as normal
    
### Submit the change to VSTGUI upstream

This is a bit tricky, and it's a bit tricky because we develop against `master` and Arne takes diffs against `develop`. So:

* `git checkout upstream/develop`
* `git checkout -b feature-branch-against-develop`
* `git merge feature-branch`

basically. Here's an actual session where I moved linux-fonts to linux-fonts-vs-develop and pushed it so I could submit
to Steinberg:

```
 2886  git fetch upstream
 2887  git checkout upstream/develop
 2888  git checkout -b linux-fonts-vs-develop

 2891  git checkout linux-fonts
 2892  git cherry -v surge
 2893  git checkout linux-fongs-vs-develop

 2895  git cherry-pick 711772715fe66bbab4e02f9fad08c9bff3356d13
 2896  git cherry -v upstream/develop
 2897  git diff upstream/develop
 2898  git push origin linux-fonts-vs-develop
```

## Historical record: How I set up the thing in the first place

* Fork `steinbergmedia/vstgui` into `surge-synthesizer`
* Set the default branch as `master`
* `git checkout -b surge`
* `git push origin surge`
* modify premake (and later cmake) to set up `vstguidir`

To apply my first diff

* get VSTGUI
* `git checkout surge`
* `git checkout -b surge-menu`
* fix
* `git commit`
* `git checkout surge`
* `git merge surge-menu`
* `git push`

Now add this as a submodule.

* `git submodule add -b surge --name vstgui.surge https://github.com/surge-synthesizer/vstgui vstgui.surge`

And write this doc and we are done!


