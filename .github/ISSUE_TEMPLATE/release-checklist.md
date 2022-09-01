---
name: Release Checklist
about: Clone this template to make a release checklist for each release
title: Release checklist for Surge XT x.y.z
labels: 'Release Plan'
assignees: ''
---

Release label: XT x.y.z
Link to milestone: https://github.com/surge-synthesizer/surge/milestone/xx

- [ ] Address all open issues for the milestone
- [ ] Update the changelog
- [ ] Update the manual
- [ ] Create git markers
  - [ ] Create a branch named `release-xt/x.y.z`
  - [ ] Sign a tag with `git tag -s release_xt_x.y.z -m "Create signed tag"`, probably after running `gpg --output x.sig --sign CMakeLists.txt` to open your GPG sig again
  - [ ] Push both the branch and tag to upstream
- [ ] Update and announce
   - [ ] Update homebrew [doc](https://github.com/surge-synthesizer/surge/tree/master/doc) 
   - [ ] Post to KvR thread, Facebook, Discord, etc.
