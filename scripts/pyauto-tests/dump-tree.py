# /usr/bin/python3

import atomacos
import sxttest

sxt = atomacos.getAppRefByBundleId("org.surge-synth-team.surge-xt")
w = sxt.windows()[0]
mf = [x for x in w.AXChildren if x.AXTitle == "Main Frame"]
sxttest.recursiveDump(mf[0], "")
