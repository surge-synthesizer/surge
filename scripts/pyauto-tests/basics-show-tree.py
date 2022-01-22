# The simplest use of the accessibility API. Grab a surge xt running instance
# and dump the accessibility hierarchy


import atomacos
import sxttest

sxt = sxttest.getSXT()
mf = sxttest.getMainFrame(sxt)
mf.activate()

sxttest.recursiveDump(mf, "")
