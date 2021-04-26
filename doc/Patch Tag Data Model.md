# Surge Patch Tag Data Model

This document describes two things. The physical (code) model for how we store tags and the
end use (value/data) model for how that physical model is used to tag patches. It does not
describe any particular implementation or tool. It is currently a RFC.

## Physical Patch Data Model

A patch has the following characteristics inside Surge:

- Gleaned from the filesystem
  - path: filesystem path to the location of the patch file (currently .fxp)
  - name: the basename without extension. `/foo/bar/This.fxp` would have `This`
- Stored in the XML with fixed format (legacy)
  - author (up to 256 chars of an author identifier)
  - comment (up to 4096 chars of plain text comment)
- A List of categorized tags
  - each patch has a list of (category, value) pair tags
  - a typed tag has a category of up to 256 chars and a value of up to 1024 chars
  - this list can be arbitrarily long.
  - The implementation places no constraints
    on tag categories or values other than that they are UTF-16 strings

## Using the Tags to generate a Logical (Content) Model

With the above flexible approach we can apply any set of key-value pairs we want
to any patch - great! That implementation choice would lead to a mess if we didn't
also define some core tag categories.

A tag category has a few features:
- Is the tag allowed multiple times in a patch or just once
- Does the tag have constrained values or free values

Here is a proposed set of tag categories.

### Tags describing the patch

Obviously, descriptive genres are not MECE. Here's a proposal.

- `factory_status`: One per patch. Has the value `factory` `third_party` or `user`
- `category`: One per patch. Prescribed values. The list of values is (still being determined) `Keys`, `Pads`, `FX`
   (this needs feedback obviously - what's the list?)
- `sub-category`: Zero or more per patch. Free values. You can tag categories with sub-categories as you
   see fit. For instance, you may want `EP` and `FM` as sub-categories of a patch in `Keys`
- `features`: Many per patch. Prescribed values. The list of values is (still being determined)
   `Layered`, `Split`, `Sequenced`, `MPE`
- `character`: Many per patch. Prescribed values. The list of values is (still being determined)
   `Cinematic`, `Analog`

### Tags for extended authorship

- `author_collection`: An author can ascribe a collection name to their bank of patches
- `author_url`: A link to author's website
- `author_license`: A license, ideally from https://spdx.org/licenses/

### User Tags

- `user_tag` Allowed as many times as you want with arbitrary content

# Example

Let's take the factory patch "Keyboards/DX EP.fxp". One would imagine tagging it like this:

- name: DX EP
- author: Claes
- factory_status: factory
- comment: Using Surge's FM capabilities, make an EP like the old DX synths
- category: Keyboards
- sub-category: EP
- sub-category: FM
- character: Digital
- character: Emulation
- author_license: GPL-3.0-or-later
- author_url: https://surge-synthesizer.github.io/

Or imagine a sequnced MPE patch I made for a project I'm working on called "ChamberSynth":

- name: That Pad From CS4
- author: baconpaul
- factory_status: user
- comment: In CS4 this is the pad under the piano
- category: Pad
- character: Mysterious
- feature: MPE
- feature: Sequenced
- user_tag: ChamberSynth Patches
- user_tag: CS4
- user_tag: 2020-07 Patches
