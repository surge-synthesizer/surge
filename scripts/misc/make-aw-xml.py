import sys
sys.path.append( "cmake-build-debug-xc" )

import surgepy
import surgepy.constants as srco

print( "<!-- ", surgepy.getVersion(), "-->" )
s = surgepy.createSurge(44100)

p = s.getPatch()
fx = p["fx"][0]
s.setParamVal(fx["type"], srco.fxt_airwindows )
s.process()

print("<!--", s.getParamDisplay(fx["type"]), "-->")

mn = s.getParamMin(fx["p"][0])
mx = s.getParamMax(fx["p"][0])

for q in range(int(mn),int(mx)):
    s.setParamVal(fx["p"][0], q )
    s.process()
    s.process()
    print( "<!--", q , "-->")
    try:
        for par in fx["p"]:
            if( s.getParamDisplay(par) != "-"):
                print( s.getParameterName(par.getId()), " ",  s.getParamDisplay(par), " [", s.getParamVal(par), "]")
    except Exception as err:
        print( "<!-- EXCEPTION ", err , "-->")
    print( " " );