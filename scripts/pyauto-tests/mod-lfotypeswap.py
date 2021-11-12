import sxttest
import time

sxt = sxttest.getSXT()
mf = sxttest.getMainFrame(sxt)
mf.activate()

sxttest.loadPatchByPath(sxt, ["Templates", "Init Sine"])
time.sleep(0.2)

lct = sxttest.firstChildByTitle(mf, "LFO Controls")
sxttest.recursiveDump(lct, "LFO>")

tad = sxttest.firstChildByTitle(lct, "LFO Type And Display")
tri = sxttest.firstChildByTitle(tad, "Triangle")
noise = sxttest.firstChildByTitle(tad, "Noise")
mseg = sxttest.firstChildByTitle(tad, "MSEG")

tri.Press()
time.sleep(0.1)
print(tad.AXValue)
time.sleep(0.5)

noise.Press()
time.sleep(0.1)
print(tad.AXValue)
time.sleep(0.5)

mseg.Press()
time.sleep(0.1)
print(tad.AXValue)
time.sleep(0.5)
