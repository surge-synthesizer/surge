# This is a python script so I can figure out how to displya temposync

import math


def fVal(inVal):
    (b, a) = math.modf(inVal)
    # print("\n", inVal, " ", a, " ", b)

    if (b < 0):
        b += 1.0
        a -= 1.0

    # print("B prior is ", b)
    b = math.pow(2.0, b)
    # print("B post is ", b)

    if (b > 1.41):
        # print("SQRT")
        b = math.log(1.5) / math.log(2.0)
    elif (b > 1.167):
        # print("167")
        b = math.log(1.3333333333) / math.log(2.0)
    else:
        # print("ZERO")
        b = 0.0

    # print("Then b = ", b)
    res = a + b
    return res


def surgeCurrent(f):
    if (f > 1):
        res = "%.3f bars" % (0.5 * math.pow(2.0, f))
    elif (f > -1):
        res = "%.3f / 4th" % (2.0 * math.pow(2.0, f))
    elif (f > -2):
        res = "%.3f / 8th" % (4.0 * math.pow(2.0, f))
    elif (f > -3):
        res = "%.3f / 16th" % (8.0 * math.pow(2.0, f))
    elif (f > -5):
        res = "%.3f / 64th" % (32.0 * math.pow(2.0, f))
    elif (f > -7):
        res = "%.3f / 128th" % (64.0 * math.pow(2.0, f))
    else:
        res = "%.3f / 256th" % (128.0 * math.pow(2.0, f))

    return res


def surgeImproved(f):
    (b, a) = math.modf(f)

    if (b >= 0):
        b -= 1.0
        a += 1.0

    if(f >= 1):
        q = math.pow(2.0, f - 1)
        nn = "whole"
        if(q >= 3):
            return "%.0lf whole notes" % (q)
        elif(q >= 2):
            nn = "double whole"
            q /= 2

        if(q < 1.3):
            t = "note"
        elif(q < 1.4):
            t = "triplet"
            if (nn == "whole"):
                nn = "1/2"
            else:
                nn = "whole"
        else:
            t = "dotted"

    else:
        d = math.pow(2.0, -(a - 2))
        q = math.pow(2.0, (b+1))
        if(q < 1.3):
            t = "note"
        elif(q < 1.4):
            t = "triplet"
            d = d / 2
        else:
            t = "dotted"
        if d == 1:
            nn = "whole"
        else:
            nn = "1/%0.d" % d

    res = "%s %s" % (nn, t)

    return res


dup = {}

for x in range(257):
    n = x / 256.0
    y = n * 14 - 11
    f = fVal(y)
    if f not in dup:
        print("%20s ==> %20s" % (surgeCurrent(fVal(y)), surgeImproved(fVal(y))))
    dup[f] = 1
