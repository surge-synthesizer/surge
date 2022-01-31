# The simplest use of the accessibility API. Grab a surge xt running instance
# and dump the accessibility hierarchy


import atomacos
import sxttest

sxt = sxttest.getSXT()
mf = sxttest.getMainFrame(sxt)
mf.activate()

tad = sxttest.firstChildByTitle(mf, "Main Menu")
tad.Press()

main = sxttest.findAllMenus(sxt)[0]
temp = sxttest.firstChildByTitle(main, "About Surge")
temp.Press()

sxttest.recursiveDump(mf, "")

sgh = sxttest.firstChildByTitle(mf, "Surge GitHub")
sgh.Press()
