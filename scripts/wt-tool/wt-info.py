import os

yn = ["no", "yes"]
fi = ["16-bit integer", "32-bit float"]
pk = ["-6 dBFS", "0 dBFS"]

for dirpath, dirnames, files in os.walk(os.getcwd()):
    for f in files:
        path = os.path.join(dirpath, f)

        if os.path.isfile(path) and path.lower().endswith(".wt"):

            with open(path, "rb") as f:
                header = {}

                header["ident"] = f.read(4)
                header["wavsz"] = f.read(4)
                header["wavct"] = f.read(2)
                header["flags"] = f.read(2)

                size  = int.from_bytes(header["wavsz"], "little")
                count = int.from_bytes(header["wavct"], "little")
                flags = int.from_bytes(header["flags"], "little")

                print(os.path.relpath(path))
                print("  frame size    : " + f"{size} samples")
                print("  frame count   : " + str(count))
                print("  is sample     : " + yn[flags &  1 == 1])
                print("  is looped     : " + yn[flags &  2 == 1])
                print("  sample format : " + fi[flags &  4 == 1])
                print("  peak value    : " + pk[flags &  8 == 1])
                print("  has metadata  : " + yn[flags & 10 == 1])
