#!/usr/bin/env python3

import os
import struct
import xml.dom.minidom


def namedCustomController(xml):
    try:
        pt = xml.getElementsByTagName("patch")[0]
        cc = pt.getElementsByTagName("customcontroller")[0]

        ent = cc.getElementsByTagName("entry")[0]

        val = ent.attributes["label"].nodeValue
        return not (val == "-")
    except:
        return False


def sceneFun(xml):
    try:
        pt = xml.getElementsByTagName("patch")[0]
        cc = pt.getElementsByTagName("scenemode")[0]
        val = cc.attributes["value"].nodeValue
        return not (val == "0")
    except:
        return False


def hasAnalogEnv(xml):
    try:
        pt = xml.getElementsByTagName("patch")[0]
        aem = 0
        aem = aem + int(pt.getElementsByTagName("a_env1_mode")[0].attributes["value"].nodeValue)
        aem = aem + int(pt.getElementsByTagName("a_env2_mode")[0].attributes["value"].nodeValue)
        aem = aem + int(pt.getElementsByTagName("b_env1_mode")[0].attributes["value"].nodeValue)
        aem = aem + int(pt.getElementsByTagName("b_env2_mode")[0].attributes["value"].nodeValue)

        if aem > 0:
            return True
        return False
    except:
        return False


def containsFX(xml):
    try:
        pt = xml.getElementsByTagName("patch")[0]
        for i in range(8):
            nm = "fx{}_type".format(i + 1)
            fx = pt.getElementsByTagName(nm)
            if len(fx) > 0:
                fxe = fx[0]
                fxt = fxe.attributes["value"].nodeValue
                if fxt == "10":
                    return True

        return False
    except:
        return False


def target(xml):
    return hasAnalogEnv(xml)


def patchToDom(patch):
    with open(patch, mode='rb') as patchFile:
        patchContent = patchFile.read()

    fxpHeader = struct.unpack(">4si4siiii28si", patchContent[:60])
    (chunkmagic, byteSize, fxMagic, version, fxId, fxVersion, numPrograms, prgName, chunkSize) = fxpHeader

    patchHeader = struct.unpack("<4siiiiiii", patchContent[60:92])
    xmlsize = patchHeader[1]
    xmlct = patchContent[92:(92 + xmlsize)].decode('utf-8')

    dom = xml.dom.minidom.parseString(xmlct)
    return dom


checks = 0
matches = 0
for dirpath, dirnames, files in os.walk("resources/data"):
    for name in files:
        if name.lower().endswith(".fxp"):
            fxp = os.path.join(dirpath, name)
            try:
                checks = checks + 1
                dom = patchToDom(fxp)
            except:
                print("FYI: ", fxp, " didn't parse")
            else:
                if target(dom):
                    matches = matches + 1
                    print(fxp + " matches")

print("Checks: ", checks, " Matches: ", matches)
