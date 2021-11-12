# INCOMPLETE

import sxttest
import time

sxt = sxttest.getSXT()
mf = sxttest.getMainFrame(sxt)
mf.activate()

sxttest.loadPatchByPath(sxt, ["Templates", "Init Sine"])
time.sleep(1)

sxttest.recursiveDump(mf, "")
