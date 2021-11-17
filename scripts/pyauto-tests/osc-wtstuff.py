import sxttest
import time
import math

sxt = sxttest.getSXT()
mf = sxttest.getMainFrame(sxt)
mf.activate()

sxttest.loadPatchByPath(sxt, ["Templates", "Init Sine"])
time.sleep(0.2)

scns = sxttest.firstChildByTitle(mf, "Active Scene")
sca = sxttest.firstChildByTitle(scns, "Scene A")
sca.Press()

osc = sxttest.firstChildByTitle(mf, "Oscillator Controls")
time.sleep(0.1)

ot = sxttest.firstChildByTitle(osc, "Oscillator Type")
ot.ShowMenu()
oscMenu = sxttest.findAllMenus(sxt)[0]
wt = sxttest.firstChildByTitle(oscMenu, "Wavetable")
wt.Press()
time.sleep(0.1)

# it is a bummer that the rebuild requires us to do this
mf = sxttest.getMainFrame(sxt)
osc = sxttest.firstChildByTitle(mf, "Oscillator Controls")
sxttest.recursiveDump(osc, "OSC Post>")
wtm = sxttest.firstChildByTitle(osc, "WT Menu");
wtm.Press()
sxttest.traverseMenuByPath(sxt, ["Sampled", "Cello"])
time.sleep(0.1)

wtjp = sxttest.firstChildByTitle(osc, "WT Next")
wtjp.Press()
time.sleep(0.1)
wtjp.Press()
time.sleep(0.1)

wtjp = sxttest.firstChildByTitle(osc, "WT Prev")
wtjp.Press()
