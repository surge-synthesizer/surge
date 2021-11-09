# /usr/bin/python3

import atomacos
import sxttest

sxt = sxttest.getSXT()
mf = sxttest.getMainFrame(sxt)
sxttest.recursiveDump(mf, "")
