# Renaming your Master branch to Main branch


Thanks to https://www.hanselman.com/blog/EasilyRenameYourGitDefaultBranchFromMasterToMain.aspx

## Upshot: If you have a fork do this

Of course, you can always re-fork but if you don't want to this prescription works

0. `git checkout master`
1. `git fetch upstream`
2. `git branch -m master main`
3. `git reset upstream/main --hard`
4. `git branch --unset-upstream`
5. `git push -u origin main`
6. Then go to your github repo. Do "Settings / Branches" and update your default branch to main.
7. On your github do "Code/ Branches" find master and press "delete"

## For the main repo do this

1. Make sure your `master` branch is up to date with origin
2. Create the main branch, moving history. `git branch -m master main`
3. Look for references to master in the code. `azure-pipelines.yml` is a particular likely spot. Also `grep -ri master . | grep -v .git` helps.
4. Push up to github. `git push -u origin main`. 
5. Change the default branch on github. ("Repo/Settings/Branches/Default Branch")
6. Delete the master branch on github. ("Code/Branches/Master/Delete")


All done


