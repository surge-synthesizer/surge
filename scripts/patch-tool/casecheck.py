# Checks the case of patches and corrects if needs be
# https://stackoverflow.com/questions/17683458/how-do-i-commit-case-sensitive-only-filename-changes-in-git
# matters a lot too Basically run this on linux if you know what you are doing

import os

fixes = {}
for dirpath, dirnames, files in os.walk("resources/data"):
    for name in files:
        dirpath = dirpath.replace("/Users/paul/music/dev/surge-clean/", "")
        if name.lower().endswith(".fxp") and not (name == "µcomputer.fxp"):
            fxp = os.path.join(dirpath, name)
            sname = name.split(" ")
            needfix = False
            for n in sname:

                start = n[0]
                if n[0].islower() and not ((n == "and") or (n == "de") or (n == "du")):
                    needfix = True

            if needfix:
                newname = ""
                prefix = ""
                for n in sname:
                    if(n[0].islower()):
                        n = n[0].upper() + n[1:]
                    newname = newname + prefix + n
                    prefix = " "

                fixes[fxp] = os.path.join(dirpath, newname)
                # print(name, "->", newname)

for k, v in fixes.items():
    cmd = "git mv --force \"{0}\" \"{1}\"".format(k, v)
    print(cmd)
    os.system(cmd)
