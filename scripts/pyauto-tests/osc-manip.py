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
on = sxttest.firstChildByTitle(osc, "Oscillator Number")
o1 = sxttest.firstChildByTitle(on, "Osc 1")
o1.Press()
time.sleep(0.1)

for i in range(4):
    o3 = sxttest.firstChildByTitle(on, "Osc 3")
    o3.Press()
    time.sleep(0.1)
    o1 = sxttest.firstChildByTitle(on, "Osc 1")
    o1.Press()
    time.sleep(0.1)

# it is a bummer that the rebuild requires us to do this
mf = sxttest.getMainFrame(sxt)
osc = sxttest.firstChildByTitle(mf, "Oscillator Controls")
sxttest.recursiveDump(osc, "OSC>")
opitch = sxttest.firstChildByTitle(osc, "Scene A Osc 1 Pitch")

for i in range(1000):
    x = i * 0.004
    v = 4.4 * math.sin(x)
    opitch.AXValue = str(v)
    time.sleep(0.002)

ot = sxttest.firstChildByTitle(osc, "Oscillator Type")
ot.Press()
oscMenu = sxttest.findAllMenus(sxt)[0]
wt = sxttest.firstChildByTitle(oscMenu, "Wavetable")
wt.Press()
time.sleep(0.1)
osc = sxttest.firstChildByTitle(mf, "Oscillator Controls")
sxttest.recursiveDump(osc, "OSC Post>")
