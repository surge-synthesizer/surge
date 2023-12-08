---
name: Release Checklist
about: Clone this template to make a release checklist for each release
title: Release checklist for Surge XT x.y.z
labels: 'Release Plan'
assignees: ''
---

Release label: XT x.y.z
Link to milestone: https://github.com/surge-synthesizer/surge/milestone/xx

## Pre-Install

- [ ] Address all open issues for the milestone
- [ ] Update the changelog
- [ ] Update the manual
- [ ] Make sure the `CMakeLists.txt` version matches the version you are about to install

## Executing the Install

- [ ] Update `releases-xt/azure-pipelines` so we don't get a tag conflict when the release happens even if it is just a bump commit
- [ ] Create git markers in the `surge` repository
  - [ ] Unlock your GPG key just in case, by running `gpg --output x.sig --sign CMakeLists.txt`
  - [ ] Create a branch named `git checkout -b release-xt/x.y.z`
  - [ ] Sign a tag with `git tag -s release_xt_x.y.z -m "Create signed tag"`,
  - [ ] Push both the branch and tag to upstream `git push --atomic upstream-write release-xt/x.y.z release_xt_x.y.z`

## Post-Install

- [ ] Update and announce
   - [ ] Update Homebrew [doc](https://github.com/surge-synthesizer/surge/tree/master/doc)
   - [ ] Update [Winget manifest](https://github.com/microsoft/winget-pkgs) (`wingetcreate update --submit --urls https://github.com/surge-synthesizer/releases-xt/releases/download/x.y.z/surge-xt-win64-x.y.z-setup.exe --version x.y.z SurgeSynthTeam.SurgeXT`)
   - [ ] Ping @luzpaz in https://github.com/surge-synthesizer/surge/issues/7132
   - [ ] Post to KvR thread, Facebook, Discord, etc.
