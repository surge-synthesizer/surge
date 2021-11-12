# Using the patch selector menu, find and press the 'open factory patches' folder

import sxttest

sxt = sxttest.getSXT()
mf = sxttest.getMainFrame(sxt)
mf.activate()

ps = sxttest.firstChildByTitle(mf, "Patch Selector")
ps.ShowMenu()

patchMenu = sxttest.findAllMenus(sxt)[0]
sxttest.recursiveDump(patchMenu, "MENU:--")

temp = sxttest.firstChildByTitle(patchMenu, "Open Factory Patches Folder...")
temp.Press()
