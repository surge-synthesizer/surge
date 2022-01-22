# The simplest use of the accessibility API. Grab a surge xt running instance
# and dump the accessibility hierarchy


import atomacos
import sxttest

sxt = sxttest.getSXT()
mf = sxttest.getMainFrame(sxt)
mf.activate()

ot = sxttest.firstChildByTitle(mf, "Scene A Play Mode")
ot.ShowMenu()
oscMenu = sxttest.findAllMenus(sxt)[0]

sxttest.recursiveDump(oscMenu, "OM>")
