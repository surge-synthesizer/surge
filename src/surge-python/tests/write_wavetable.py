"""
Tests for Surge XT Python bindings.
"""
import sys
sys.path.append('/Users/paul/dev/music/surge/cmake-build-debug/src/surge-python/')
import surgepy

def main():
    s = surgepy.createSurge(44100)

    patch = s.getPatch()
    osc0 = patch["scene"][0]["osc"][0]
    otype = osc0["type"]
    s.setParamVal(otype, surgepy.constants.ot_wavetable)
    s.loadWavetable(0, 0, "/Users/paul/dev/music/surge/resources/data/wavetables_3rdparty/A.Liv/Droplet/Droplet 2.wav")
    s.savePatch("/Users/paul/Desktop/PythonGenerated.fxp")

if __name__ == "__main__":
    main()