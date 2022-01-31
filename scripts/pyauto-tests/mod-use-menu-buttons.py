# Show that that the modulation menu properly exposes to accessibility
# by dumping the menu, muting it, then dumping it again and unmuting it

import sxttest
import time

sxt = sxttest.getSXT()
mf = sxttest.getMainFrame(sxt)
mf.activate()

# Load DXEP
sxttest.loadPatchByPath(sxt, ["Keys", "DX EP"])
time.sleep(0.5)

pfg = sxttest.firstChildByTitle(mf, "Scene A Pre-Filter Gain")
pfg.ShowMenu()
pfgMenu = sxttest.findAllMenus(sxt)[0]
sxttest.recursiveDump(pfgMenu, "MENU>> ")
moded = sxttest.firstChildByTitle(pfgMenu, "Mute ENV 3");
moded.Press()

time.sleep(0.5)

pfg.ShowMenu()
pfgMenu = sxttest.findAllMenus(sxt)[0]
sxttest.recursiveDump(pfgMenu, "MENU>> ")
moded = sxttest.firstChildByTitle(pfgMenu, "UnMute ENV 3");
moded.Press()
