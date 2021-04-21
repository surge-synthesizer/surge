This document is just @baconpaul's reminder to himself how he updates homebrew. 
It presumes you have brew installed and up to date. It is sparse, but a track of what he did
for 1.6.2.

Follow the docs and fork https://github.com/Homebrew/homebrew-cask 

```
brew update 
cd $(brew --repo homebrew/cask)
cd Casks
git remote -v
```

at this point I see my fork is set up

```
baconpaul	git@github.com:baconpaul/homebrew-cask.git (fetch)
baconpaul	git@github.com:baconpaul/homebrew-cask.git (push)
origin	https://github.com/Homebrew/homebrew-cask (fetch)
origin	https://github.com/Homebrew/homebrew-cask (push)
```

great so since I brew updated my `baconpaul` will be behind

```
git checkout master
git pull origin
```

then make a branch to push to baconpaul

```
git checkout -b surge-(version)
```

Modify `surge-synthesizer.rb` appropriately and test. You primarily need to update the version number and the SHA256 hash.

```
openssl dgst -sha256 (dmg)
vi surge-synthesizer.rb
brew audit --cask --online surge-synthesizer.rb 
brew style --cask surge-synthesizer.rb
```

commit and push then sweep the PR to homebrew, and their machinery will take over.



