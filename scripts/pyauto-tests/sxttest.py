import atomacos


def getSXT():
    sxt = atomacos.getAppRefByBundleId("org.surge-synth-team.surge-xt")
    return sxt


def getMainFrame(sxt):
    w = sxt.windows()[0]
    mf = [x for x in w.AXChildren if x.AXTitle == "Main Frame"]
    return mf[0]


def recursiveDump(w, pf):
    l = "{0} '{1}' ({2} actions={3})".format(pf, w.AXTitle, w.AXRole, w.getActions())
    try:
        r = " value=" + w.AXValue
        l += r
    except:
        pass
    print(l)
    kids = w.AXChildrenInNavigationOrder
    for k in kids:
        recursiveDump(k, pf + "|--")


# obviously not very efficient. Should pass accumulator etc
def filterChildren(w, fil):
    res = []
    if (fil(w)):
        res.append(w)
    kids = w.AXChildrenInNavigationOrder
    for k in kids:
        rk = filterChildren(k, fil)
        for nk in rk:
            res.append(nk)
    return res


def firstChildByTitle(parent, title):
    return filterChildren(parent, lambda w: w.AXTitle == title)[0]


def findAllMenus(sxt):
    res = []
    for q in sxt.AXChildren:
        try:
            if (q.AXTitle == "menu"):
                res.append(q)
        except:
            pass
    return res


def loadPatchByPath(sxt, path):
    mf = getMainFrame(sxt)
    ps = firstChildByTitle(mf, "Patch Selector")
    ps.ShowMenu()
    for p in path:
        patchMenu = findAllMenus(sxt)[0]
        temp = firstChildByTitle(patchMenu, p)
        temp.Press()


def traverseMenuByPath(sxt, path):
    for p in path:
        patchMenu = findAllMenus(sxt)[0]
        temp = firstChildByTitle(patchMenu, p)
        temp.Press()
