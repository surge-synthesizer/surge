import os

for dirpath, dirnames, files in os.walk("resources/data"):
    for name in files:
        if name.lower().endswith(".wt"):
            wt = os.path.join(dirpath, name)
            with open(wt, "rb") as f:
                header = {}
                header["ident"] = f.read(4)
                # FIXME: Check that this is actually vawt

                header["wavsz"] = f.read(4)
                header["wavct"] = f.read(2)
                header["flags"] = f.read(2)

                print( wt, " ", header["flags" ][0], header["flags"][1] );
