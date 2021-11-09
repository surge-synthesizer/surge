import sxttest
import time

sxt = sxttest.getSXT()
mf = sxttest.getMainFrame(sxt)
mf.activate()

# test the 'depth 1' patch menus
patches = [["Leads", "Sharpish"],
           ["Sequences", "Gate Chord"],
           ["Keys", "DX EP"],
           ["Plucks", "Bell 1"]
           ]
for patchPair in patches:
    print("LOADING ----", patchPair)
    ps = sxttest.firstChildByTitle(mf, "Patch Selector")
    ps.ShowMenu()
    patchMenu = sxttest.findAllMenus(sxt)[0]

    temp = sxttest.firstChildByTitle(patchMenu, patchPair[0])
    temp.Press()

    subMen = sxttest.findAllMenus(sxt)[0]
    loadPatch = sxttest.firstChildByTitle(subMen, patchPair[1])
    loadPatch.Press()
    time.sleep(2)
