# Load patches by a path. The function that is explicitly tested here
# is also available in sxttest as a helper function

import sxttest
import time

sxt = sxttest.getSXT()
mf = sxttest.getMainFrame(sxt)
mf.activate()

patches = [["Leads", "Sharpish"],
           ["Sequences", "Gate Chord"],
           ["Keys", "DX EP"],
           ["Plucks", "Bell 1"]
           ]
ps = sxttest.firstChildByTitle(mf, "Patch Selector")

for patchPair in patches:
    print("LOADING ----", patchPair)
    ps.ShowMenu()
    patchMenu = sxttest.findAllMenus(sxt)[0]

    temp = sxttest.firstChildByTitle(patchMenu, patchPair[0])
    temp.Press()

    subMen = sxttest.findAllMenus(sxt)[0]
    loadPatch = sxttest.firstChildByTitle(subMen, patchPair[1])
    loadPatch.Press()
    time.sleep(0.5)
