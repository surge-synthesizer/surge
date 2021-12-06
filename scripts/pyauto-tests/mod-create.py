# Demonstrates creating modulations and so forth

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
select = sxttest.firstChildByTitle(l1, "Select");
select.Press()
time.sleep(1)

l1 = sxttest.firstChildByTitle(mods, "Keytrack");
select = sxttest.firstChildByTitle(l1, "Select");
select.Press()
time.sleep(1)

od = sxttest.firstChildByTitle(mf, "Scene A Osc Drift")
od.AXValue = "32"
time.sleep(0.5)

co = sxttest.firstChildByTitle(mf, "Scene A Filter 1 Cutoff")
co.AXValue = "440"
time.sleep(0.5)

sxttest.recursiveDump(l1, "Keytrack PRE>")
arm = sxttest.firstChildByTitle(l1, "Arm");
arm.Press()
time.sleep(1)
sxttest.recursiveDump(l1, "Keytrack POST>")

l2 = sxttest.firstChildByTitle(mods, "LFO 3")
tg = sxttest.firstChildByTitle(l2, "Target")
tg.Press()
time.sleep(0.5)

od.AXValue = "23"
time.sleep(0.5)

co.AXValue = "880"
time.sleep(2.5)

arm.Press()
