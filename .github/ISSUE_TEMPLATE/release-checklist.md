---
name: Release Checklist
about: Clone this template to make a release checklist for each release
title: Release Checklist for version A.B.C
labels: ''
assignees: ''

---

Release label: A.B.C
Target Release Date: 
Milestone

- [ ] Address all open issues for the milestone
- [ ] Move the surge-fx pointer and push
- [ ] Move the surge-rack pointer and update rack
- [ ] Update the changelog
- [ ] Update Manual
- [ ] Create git markers
  - [ ] create a branch named `release/1.6.3` or whatnot
  - [ ] sign a tag with `git tag -s release_1.6.3 -m "Create signed tag"`
  - [ ] push both the branch and tag to upstream
- [ ] Update and Announce
   - [ ] Update home-brew [doc](https://github.com/surge-synthesizer/surge/tree/master/doc) 
   - [ ] Post to KVR thread, Facebook, Slack etc
   - [ ] Notify sonisto at rune.braager@sonisto.com
