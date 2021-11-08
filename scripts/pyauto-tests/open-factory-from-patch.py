import sxttest
import time

sxt = sxttest.getSXT()
mf = sxttest.getMainFrame(sxt)
mf.activate()

ps = sxttest.filterChildren(mf, lambda w: w.AXTitle == "Patch Selector")[0]
print("Showing Menu")
ps.ShowMenu()

time.sleep(2)
print("About to click")

menus = sxttest.findAllMenus(sxt)
patchMenu = menus[0]

temp = sxttest.filterChildren(patchMenu, lambda w: w.AXTitle == "Open Factory Patches Folder...")[0]
temp.Press()
