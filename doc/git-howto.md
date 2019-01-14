# Using git with Surge

## Disclaimer

git is a powerful and multifaceted tool. The ability to move chunks of commits around, to label them as branches, and to
have repositories in various states of sync and not, give you amazing flexilibity. They also result in it being a tool
where there is way more than one way to do something. So this document is not a depositive guide to git.

Rather this document is a short set of answers to things that some of the surge devs keep getting asked in the slack
or on issues, put in once place, for your help.

If you think there's a better way to do things in this document, that's cool! Do them. If you think the document would be
better by having them included in this document, that's cool! This document literally tells you how to change it and make
a pull request. If you would like to share feedback about how your way is better and this is, in some sense, the Worst Git Guide Ever,
hey maybe use that energy to close a bug instead! And with that said, onwards!

## Basics

Make a github account, fork [kurasu/surge](https://www.github.com/kurasu/surge), and check it out onto your machine. Do a build.
Does it work? If so, great you are started! You also have git working properly (and presumably have it in your path in windows or
are using git-bash there).

## Update your fork with master

Often something will be changed in `kurasu/surge` and you will also want that in your copy. To do this you need to merge
your upstream into the master. [Github has good documentation on this](https://help.github.com/articles/syncing-a-fork/#platform-all)
but here's the steps

### Onetime only - add kurasu/surge as an upstream remote

git works on the concept of "remotes", other places where you can get or push commits. When you check out you get a remote
called "origin" which is the copy on github.com of the git repo now on your hard drive. You can add lots of other remotes also.

You can do lots of operations on remotes, but the two big ones are `fetch` which bring all the diffs from the remote down to you
and `push` which push all the diffs from you to a remote. 

You need to configure these remotes one time per remote. So do this:

```
git remte add upstream https://github.com/kurasu/surge
git remote -v
```

and you should see something like

```
origin	git@github.com:baconpaul/surge.git (fetch)
origin	git@github.com:baconpaul/surge.git (push)
upstream	https://github.com/kurasu/surge (fetch)
upstream	https://github.com/kurasu/surge (push)
```

### Every time you want to update

Your actions are to fetch upstream, merge that upstream into your master, and then push that out to your github.

```
git fetch upstream   # You now have the diffs in kurasu/surge
git checkout master  # you now are on your copy of master
git merge upstream/master # you now have integrated the upstream diffs into your copy 
git push origin master # you now have pushed your updated master back to github
```

## Developing

OK so now you want to change your local copy somehow. There is one golden rule of doing this.

**Never change your local copy in the master branch. Always make a branch for development**

If you break this rule, keeping your master in sync with the upstream master is a pain. There's
a way around it, but it is a bit scary.

### Changing code on a feature branch

Think of the name of the change you are going to make. Some of the devs also include a github
issue number in the name. So if you are fixing automation in github issue 247 you would use a name
like `automation-247`. Then create that branch as follows

```
git checkout master
git checkout -b automation-247
```

Now you can develop to your hearts desire. When you are done developing you can commit. But at this
point the branch is still just on your local hard drive. So you need to push it back to orgin.

```
git push origin automation-247
```

and at this point, you can make a pull request.

### Branches can come and go

Think of branches as labels on commits. You can make them all the time, commit them, push them, make a
pull request, and then delete them (`git branch -d my-branch-name`) if you want. Feel free to make a new
and different branch for each pull request.

### I would like to try some other developers branch

Lets say that a developer says in slack something like "I've fixed this, but it is on my someuser this-fix-923 branch"
where someuser is their git id and that's the branch name. 

Well to do this, you just need someuser as another remote, just like upstream became. So one time do

```
git remote add someuser https://www.github.com/someuser/surge
```

then when you want their feature do

```
git fetch someuser
git checkout someuser/this-fix-923
```

this will give you a 'detatched HEAD' message. Don't wory about that. It's beyond the scope of this exercise.
And of course, to go back to your version you can just `git checkout master`.


### I broke the golden rule and developed in master. Help me fix it

Sigh. OK be careful. You are about to lose all your changes in master. Back them up somehow.
Then:

```
git fetch upstream
git checkout master
git reset --hard upstream/master
git push origin master --force
```

More than enough people who end up in this situation also find [this stackoverflow answer](https://stackoverflow.com/questions/1628088/reset-local-repository-branch-to-be-just-like-remote-repository-head)
to be very useful.


