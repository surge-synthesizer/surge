import sxttest
import time

sxt = sxttest.getSXT()
mf = sxttest.getMainFrame(sxt)
mf.activate()

# Load DXEP
ps = sxttest.firstChildByTitle(mf, "Patch Selector")
ps.ShowMenu()
patchMenu = sxttest.findAllMenus(sxt)[0]
temp = sxttest.firstChildByTitle(patchMenu, "Keys")
temp.Press()
subMen = sxttest.findAllMenus(sxt)[0]
loadPatch = sxttest.firstChildByTitle(subMen, "DX EP")
loadPatch.Press()
time.sleep(1)

pfg = sxttest.firstChildByTitle(mf, "Scene A Pre-Filter Gain")
pfg.ShowMenu()
pfgMenu = sxttest.findAllMenus(sxt)[0]
sxttest.recursiveDump(pfgMenu, "MENU>> ")

# moded = sxttest.firstChildByTitle(pfgMenu, "ENV 3 by 4.63 dB");
# moded.Press()

moded = sxttest.firstChildByTitle(pfgMenu, "Mute ENV 3");
moded.Press()

time.sleep(1)

pfg.ShowMenu()
pfgMenu = sxttest.findAllMenus(sxt)[0]
sxttest.recursiveDump(pfgMenu, "MENU>> ")
