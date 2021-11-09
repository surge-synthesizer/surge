import sxttest

sxt = sxttest.getSXT()
mf = sxttest.getMainFrame(sxt)
mf.activate()

ps = sxttest.firstChildByTitle(mf, "Patch Selector")
ps.ShowMenu()

menus = sxttest.findAllMenus(sxt)
patchMenu = menus[0]

sxttest.recursiveDump(patchMenu, "MENU:--")

temp = sxttest.firstChildByTitle(patchMenu, "Open Factory Patches Folder...")
temp.Press()
