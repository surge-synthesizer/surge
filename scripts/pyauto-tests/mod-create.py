# INCOMPLETE

import sxttest
import time

sxt = sxttest.getSXT()
mf = sxttest.getMainFrame(sxt)
mf.activate()

sxttest.loadPatchByPath(sxt, ["Templates", "Init Sine"])
time.sleep(1)

mods = sxttest.firstChildByTitle(mf, "Modulators")

sxttest.recursiveDump(mods, "MOD>")
l1 = sxttest.firstChildByTitle(mods, "LFO 1");
arm = sxttest.firstChildByTitle(l1, "Select");
arm.Press()
arm.Press()

sxttest.recursiveDump(mf, "TOP>");
