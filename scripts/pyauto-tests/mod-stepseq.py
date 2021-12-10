import sxttest
import time
import random

sxt = sxttest.getSXT()
mf = sxttest.getMainFrame(sxt)
mf.activate()
time.sleep(1)

sxttest.loadPatchByPath(sxt, ["Templates", "Init Sine"])
# sxttest.loadPatchByPath(sxt, ["Test Cases", "StepAcc"])
time.sleep(0.2)

mods = sxttest.firstChildByTitle(mf, "Modulators")
l1 = sxttest.firstChildByTitle(mods, "LFO 1");
select = sxttest.firstChildByTitle(l1, "Select");
select.Press()

lct = sxttest.firstChildByTitle(mf, "LFO Controls")
sxttest.recursiveDump(lct, "LFO>")

tad = sxttest.firstChildByTitle(lct, "LFO Type And Display")
stp = sxttest.firstChildByTitle(tad, "Step Sequencer")

stp.Press()
time.sleep(0.1)
print(tad.AXValue)
time.sleep(0.5)

sxttest.recursiveDump(tad, "TAD>")

for i in range(20):
    q = random.randint(1, 16)
    v = random.uniform(-1, 1)
    nm = "Step Value " + str(q)
    step = sxttest.firstChildByTitle(lct, nm)
    step.AXValue = str(v)
    time.sleep(0.2)

mods = sxttest.firstChildByTitle(mf, "Modulators")
l1 = sxttest.firstChildByTitle(mods, "S-LFO 1");
select = sxttest.firstChildByTitle(l1, "Select");
select.Press()
time.sleep(0.2)
stp = sxttest.firstChildByTitle(tad, "Step Sequencer")
stp.Press()
time.sleep(0.2)
sxttest.recursiveDump(tad, "SLFO>")
