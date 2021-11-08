import atomacos


def recursiveDump(w, pf):
    l = "{0} '{1}' ({2} actions={3})".format(pf, w.AXTitle, w.AXRole, w.getActions())
    print(l)
    kids = w.AXChildrenInNavigationOrder
    for k in kids:
        recW(k, pf + "|--")
