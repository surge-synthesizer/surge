---
name: Release Checklist
about: Clone this template to make a release checklist for each release
title: Release checklist for Surge XT x.y.z
labels: 'Release Plan'
assignees: ''
---

Release label: XT x.y.z
Link to milestone:

- [ ] Address all open issues for the milestone
- [ ] Update the changelog
- [ ] Update the manual
- [ ] Create git markers
  - [ ] create a branch named `release-xt/x.y.z`
  - [ ] sign a tag with `git tag -s release_1.6.3 -m "Create signed tag"`, probably after running `gpg --output x.sig --sign CMakeLists.txt` to open your GPG sig again
  - [ ] push both the branch and tag to upstream
- [ ] Update and announce
   - [ ] Update home-brew [doc](https://github.com/surge-synthesizer/surge/tree/master/doc) 
   - [ ] Post to KvR thread, Facebook, Discord, etc.
