# Some tips for developing on Surge

As we've done several pull requests and reviews, a group of developers have convened on this set of 
sort of standards for developing with surge. None of these are set in stone but PR reviewers will
have an easier time if you skew close to them.

And hop on slack anytime if you have questions. Asking before you start is welcome.

## Code Review Culture

Code reviews are important parts of pull requests. Folks do them carefully and
seriously. But we always are polite and professional. If you think a comment about
your code is somehow rude or impolite, please don't; noone in the surge community has that 
intent.

But please do expect your code to be reviewed before it is merged, and you may be asked
to make changes. Skewing to these rules nad checking them before you do a PR can help 
make everyones time more efficient.

## git

* The [git-howto](git-howto.md) is a good basic if you are newer to git.
* Generally, try and have a github issue which documents what you are trying to do
* Name your branch with a descriptive name and an isssue number. For isntance
`mac-wav-file-198` is a branch to implement wav file reading on mac in response to issue
198
* Try to have pull requests have one or a small number of commits. If your change has two
logical big steps, two commits is fine, but work commits where you just make a small change
could be squashed with `git rebase`. If you don't know how to do this, ask on slack or 
put in the PR with your commits and we can squash at merge
* Try to avoid trivial or unrelated diffs. Your diffs should be "about" the same size as your
change.

## Code Basics

* Obey the 'campground rule' that code you change should get better. Cleaner, better names,
better indents, more comments.
* Use spaces, not tabs
* The code is mostly formatted with 3 space tab width, which is odd. If you are in a 3-space
section stick to 3 spaces. If you are writing new code we are OK with 3 or 4. But code should align

## Naming

* `#defined` constants are `UPPERCASE_VARIABLES`
* `class HaveCamelcaseNames`
* `void functionsAreCamelCaseWithLowerFirst`
* We are not using `s_` or `m_` or equivalent notations for members or statics
* Full namespaces are generally prefered over usings. Even though the code has a `using std` in globals.h we are trying to use
`std::vector` over `vector` in the code. 
* Use namespaces not classes to group functions. Check out how we implemented `UserInteractions.h`
* Long and descriptive names are good. `userMessageDeliveryPipe` is better than `umdp`. 

## Comments

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

## Miscellany

Occasionally code needs an `#if MAC` but if you have entire classes with parallel implementations
just put the implementations in the `src/mac` `src/windows` and `src/linux` directories and
let premake pick the right one. This does mean that you need stubs on all three platforms to link.
Look at the `UserInteractions` example.

The only numbers which make sense in code are 0, 1, n, and infinity. If you are using a number other
than that, perhaps toss in a comment.

Prefer `std::ostringstream` and so on to `sprintf` and so on.


