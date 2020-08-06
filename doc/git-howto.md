# Using git with Surge

## Disclaimer

Git is a powerful and multifaceted tool. The ability to move chunks of commits around, to label them as branches, and to
have repositories in various states of sync and not, give you amazing flexilibity. They also result in it being a tool
where there is way more than one way to do something. So this document is not a depositive guide to git.

Rather this document is a short set of answers to things that some of the surge devs keep getting asked in the slack
or on issues, put in once place, for your help.

If you think there's a better way to do things in this document, that's cool! Do them. If you think the document would be
better by having them included in this document, that's cool! This document literally tells you how to change it and make
a pull request. If you would like to share feedback about how your way is better and this is, in some sense, the Worst Git Guide Ever,
hey maybe use that energy to close a bug instead! And with that said, onwards!

## Before you read this document, do a build!

This document talks about how to make changes to Surge, develop new features, and submit them as Pull Requests. It also talks
about coding standards and reviews a bit. But it pre-supposes that you have built Surge once.

The directions on how to build surge once are on our [README](https://github.com/surge-synthesizer/surge). Before you read that doc,
please follow those directions, run the resulting synth, and make sure that the about screen shows it having been built on your system.
Those directions are pretty bulletproof at this point, but if you find an error, please do let us know.

And with that, onwards!

## Basics

Register a new GitHub account, log in with your new account and go to [https://github.com/surge-synthesizer/surge](https://github.com/surge-synthesizer/surge). On the top right corner you will see a bunch of buttons, one of which says `Fork`. When you click on this, you will create a new fork of Surge onto your GitHub account.

This means that you can create a new `branch` in your `fork`, `push` some changes into the branch and later make a `Pull Request` (PR) out of it (towards `surge-synthesizer/surge`), which the Surge-maintainers can review, and, when approved, it will become a part of the actual Surge codebase.

Now that you have clicked on `Fork` - a brand new fork of the Surge repository is in existence. In your case, replace the `example-user` in the following addresses with your actual GitHub account name. `example-user/surge` does not exist and will not exist. Use `<yourusernamehere>/surge`, and throw out the `<` and the `<`. 

Your fork will live in this URL: http://github.com/example-user/surge

The .git file that you can clone, if you so choose, is http://github.com/example-user/surge.git.

However, since you have already cloned the `surge-synthesizer/surge` onto your device, you can just add it as a remote. So type in `git remote add example-user https://github.com/example-user/surge.git` - again, please be sure to **replace** the `example-user` in both portions of that commandline entry with **your** username. After this, you can do a `git remote -vv` and, if you like type `git fetch example-user` (again remembering to **replace** `example-user` with **your** username.

## Update your fork with master

Often something will be changed in `surge-synthesizer/surge` and you will also want that in your local copy. To do this you need to merge
your upstream into the master. [Github has good documentation on this](https://help.github.com/articles/syncing-a-fork/#platform-all)
but here are the steps

### Onetime only - add surge-synthesizer/surge as an upstream remote

git works on the concept of "remotes", other places where you can get or push commits. When you check out you get a remote
called "origin" which is the copy on github.com of the git repo now on your hard drive. You can add lots of other remotes also.

You can do lots of operations on remotes, but the two big ones are `fetch` which bring all the diffs from the remote down to you
and `push` which push all the diffs from you to a remote. 

You need to configure these remotes one time per remote. So do this:

```
git remote add upstream https://github.com/surge-synthesizer/surge
git remote -v
```

and you should see something like

```
origin	git@github.com:baconpaul/surge.git (fetch)
origin	git@github.com:baconpaul/surge.git (push)
upstream	https://github.com/surge-synthesizer/surge (fetch)
upstream	https://github.com/surge-synthesizer/surge (push)
```

### Every time you want to update

Your actions are to fetch upstream, reset your master to match upstream, and then push that out to your github.

```
git fetch upstream   # You now have the diffs in surge-synthesizer/surge
git checkout master  # you now are on your copy of master
git reset upstream/master --hard # you now have integrated the upstream diffs into your copy 
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

Lets say, that a developer says on Slack something to the tone of "I've fixed (this issue), but it is on my example-user example-branch branch".
In this case, the `example-user` will be their actual git id, and `example-branch` will be the branch-name that they have the fix in. 

Well, to do this, you just need to add `example-user` as another remote, just like upstream was. So, do this:

```
git remote add example-user https://www.github.com/example-user/surge
```

then, when you want their branch, write

```
git fetch example-user
git checkout example-user/example-branch
```

This will give you a 'detached HEAD' message. Don't wory about that. It's beyond the scope of this exercise.
And of course, to go back to your version you can just type `git checkout master`.


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

## Squashing commits

Often the number of commits you do in a branch is more than the number of actual changes. For instance a typical 
pattern would be develop on mac, do a commit, then test it on windows and linux. On linux if you find an error you 
make a one line change and commit that. Now your change, which is still "one thing" has two commits.

Or perhaps you develop your code over a week committing regularly and all those intermediate work commits
aren't ones you wnat to make visibile. Or perhaps you develop then format as a last step and have changes.

In any case, as maintainers, we would sooner have a small number of git commits in our Pull Requests.

To accomplish this you can squash your commits into a single commit and write a good message. To do this
you use `git rebase` which is generally a terrifying command, but in interactive mode, is not as bad.

The easiest approach is, with your branch checked out and committed, to run `git rebase -i master`. This will
popup a first editor with all your commits. Have one set as "pick" (usually the first one) and set the rest to
"squash". (There are other settings of course, but a use case of flatten to one commit is best done this way).
Change the others to "squash" by just editing and writing the file. Then you get a change to restate your commit
message with the default being the union of commit messages. Rewrite and save as normal.

Then `git push origin my-branch --force` will send your newly based branch up to github. Do a pull request and you
should see one commit with your lovely new message.


### An explicit example: Squashing Commits for Code Review Feedback

Lets imagine you have developed a branch and pushed it up and made a pull request and you get code review feedback.
You would like to integrate that feedback but that's going to make another commit. Do you need a new branch and a new 
pull request? No absolutely not! You can update in place and keep one clean commit. Here's how you do it.

As a starting point, we have our branch we have submitted for a pull request with one commit. You want to integrate your
concerns so you check out the branch again. At the outset you should see this:

```
paul:~/dev/music/surge$ git checkout update-git-howto
Already on 'update-git-howto'
paul:~/dev/music/surge$ git cherry -v master
+ 452b43d5e78119af80c68d0b2bf2ab47291ae3fd Add a section on code review rebase
paul:~/dev/music/surge$ git status
On branch update-git-howto
```

One commit on a branch. Great now you go ahead and make your changes. Code, test, etc... and then your changes are
ready to commit. 

```
paul:~/dev/music/surge$ git add doc/
paul:~/dev/music/surge$ git commit -m "Integrate Code Review Comments from @user"
[update-git-howto d41af35] Integrate Code Review Comments from @user
 1 file changed, 14 insertions(+)
paul:~/dev/music/surge$ git cherry -v master
+ 452b43d5e78119af80c68d0b2bf2ab47291ae3fd Add a section on code review rebase
+ d41af35a0e4e9658a3bca6c593b5ec33c0b4848f Integrate Code Review Comments from @user
```

Now at this point you could push your branch (`git push origin update-git-howto`) and the PR would update
and would have two commits in it. The continous integration woudl run and voila. But the maintainers would probably
squash those commits and rewrite your commit message. So you may want to squash down to one commit. Here's how you do it.

First rebase interactively with `git rebase -i master`. You will see an editor which looks like this:


```
pick 452b43d Add a section on code review rebase
pick d41af35 Integrate Code Review Comments from @user

# Rebase fdf935b..d41af35 onto fdf935b (2 commands)
#
```

What rebase is going to do is to collapse or drop commits in the path. You want to keep the first commit and
squash the second (which will mean you get one commit containing the changes of both). Change the file to say this

```
pick 452b43d Add a section on code review rebase
squash d41af35 Integrate Code Review Comments from @user

# Rebase fdf935b..d41af35 onto fdf935b (2 commands)
#
```

And then save it. You will then get an opportunity to rewrite your commit message. The default message will be
the union of the message of the two commits. But edit and save and you should see

```
paul:~/dev/music/surge$ git cherry -v master
+ c9ccb49e8c7972457a22a67ee3b799bb7a55f420 Add a section on code review rebase
```

Note the commit id (here c9cc and so on) is a new id which wasn't in the prior commit. That makes sense. Git has made a 
completely new transaction for the branch which is the union. So now you just need to update your branch. Since the commit
history has changed you need to force. So

```
git push origin my-branch-name --force
```

and the PR page will say "@user force pushed branch" and everything can proceed.
