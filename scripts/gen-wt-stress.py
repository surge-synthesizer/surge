#!/usr/bin/env python3
"""
Generate a large stress-test wavetable directory structure under
~/Documents/Surge Synth Team/Surge XT/Wavetables/test-scale

Structure: 4 levels deep, 3-4 wide, ~200k wavetable copies in ~1k leaf dirs.
"""

import os
import shutil
import random
import string

SOURCE_WT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                         "resources", "data", "wavetables", "Basic", "Sine Octaves.wt")

BASE_DIR = os.path.expanduser(
    "~/Documents/Surge Synth Team/Surge XT/Wavetables/test-scale"
)

ADJECTIVES = [
    "bright", "dark", "warm", "cold", "soft", "hard", "deep", "thin",
    "rich", "pure", "raw", "smooth", "sharp", "round", "flat", "wide",
    "narrow", "dense", "sparse", "heavy", "light", "fast", "slow", "high",
    "low", "loud", "quiet", "clean", "dirty", "crisp"
]

NOUNS = [
    "wave", "tone", "pulse", "ring", "bell", "pad", "lead", "bass",
    "drum", "pluck", "bow", "blow", "hit", "sweep", "drone", "chord",
    "arp", "seq", "mod", "lfo", "env", "osc", "vco", "vcf", "vca",
    "fx", "rev", "del", "cho", "fla"
]

random.seed(42)

def unique_name(used, pool_a, pool_b, suffix=""):
    while True:
        name = random.choice(pool_a) + "-" + random.choice(pool_b) + suffix
        if name not in used:
            used.add(name)
            return name

def make_dir_tree(base, depth=4, width_range=(3, 4)):
    """Recursively create directories; return list of leaf dirs."""
    if depth == 0:
        return [base]
    leaves = []
    used = set()
    width = random.randint(*width_range)
    for _ in range(width):
        name = unique_name(used, ADJECTIVES, NOUNS)
        child = os.path.join(base, name)
        os.makedirs(child, exist_ok=True)
        leaves.extend(make_dir_tree(child, depth - 1, width_range))
    return leaves

def main():
    if not os.path.isfile(SOURCE_WT):
        print(f"ERROR: source wavetable not found: {SOURCE_WT}")
        return

    print(f"Creating directory tree under: {BASE_DIR}")
    os.makedirs(BASE_DIR, exist_ok=True)

    leaves = make_dir_tree(BASE_DIR, depth=4, width_range=(3, 4))
    print(f"Created {len(leaves)} leaf directories")

    # Distribute ~200k copies across leaf dirs
    target_total = 200_000
    copies_per_leaf = max(1, target_total // len(leaves))

    print(f"Copying ~{copies_per_leaf} wavetables into each leaf dir "
          f"(target total: ~{copies_per_leaf * len(leaves):,})")

    total = 0
    wt_nouns = [
        "alpha", "beta", "gamma", "delta", "epsilon", "zeta", "eta", "theta",
        "iota", "kappa", "lambda", "mu", "nu", "xi", "omicron", "pi",
        "rho", "sigma", "tau", "upsilon", "phi", "chi", "psi", "omega"
    ]

    for leaf in leaves:
        used = set()
        for i in range(copies_per_leaf):
            # Build a unique filename per leaf
            fname = f"wt-{i:05d}-" + random.choice(ADJECTIVES) + "-" + random.choice(wt_nouns) + ".wt"
            dest = os.path.join(leaf, fname)
            shutil.copy2(SOURCE_WT, dest)
            total += 1

        if total % 10_000 == 0:
            print(f"  {total:,} files written...")

    print(f"Done. Total wavetables written: {total:,} across {len(leaves)} directories.")

if __name__ == "__main__":
    main()
