# Load init sine and then set up a couple of effects using the menus
# and so on

import sxttest
import time

sxt = sxttest.getSXT()
mf = sxttest.getMainFrame(sxt)
mf.activate()

sxttest.loadPatchByPath(sxt, ["Templates", "Init Sine"])
time.sleep(0.2)

fxr = sxttest.firstChildByTitle(mf, "FX Controls")
sxttest.recursiveDump(fxr, "FX>> ")

slt = sxttest.firstChildByTitle(fxr, "FX Slots")
print("SLOT VALUE : ", slt.AXValue)
sxttest.recursiveDump(slt, "SLOT>")

f1 = sxttest.firstChildByTitle(slt, "Send FX 1")
f1.Press()
print("SLOT VALUE : ", slt.AXValue)
time.sleep(0.2)

fxtype = sxttest.firstChildByTitle(fxr, "FX Type")
fxtype.ShowMenu()
path = ["Chorus", "Fat"]
for p in path:
    patchMenu = sxttest.findAllMenus(sxt)[0]
    temp = sxttest.firstChildByTitle(patchMenu, p)
    temp.Press()

a1 = sxttest.firstChildByTitle(slt, "A Insert FX 1")
a1.Press()
print("SLOT VALUE : ", slt.AXValue)
time.sleep(0.2)
fxtype.ShowMenu()
path = ["Flanger", "Fast"]
for p in path:
    patchMenu = sxttest.findAllMenus(sxt)[0]
    temp = sxttest.firstChildByTitle(patchMenu, p)
    temp.Press()
