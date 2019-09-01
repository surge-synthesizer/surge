import os

for dirpath, dirnames, files in os.walk("resources/data"):
    for name in files:
        if name.lower().endswith(".wt"):
            wt = os.path.join(dirpath, name)
            with open(wt, "rb") as f:
                f.read(4)
                wsize = int.from_bytes(f.read(4), 'little')
                if(wsize == 128):
                    print(wt)
