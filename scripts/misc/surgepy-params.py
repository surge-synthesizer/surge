import sys
sys.path.append( "cmake-build-release/src/surge-python/" )

import surgepy

surge = surgepy.createSurge(48000)
patch = surge.getPatch()

def set_osc_type(patch, ot):
    osc_type = patch['scene'][0]['osc'][0]['type']
    surge.setParamVal(osc_type, ot)

def get_osc_params(patch):
    osc = patch['scene'][0]['osc'][0]
    osc_type = osc['type']
    osc_contextual_params = osc['p']
    print(f"Oscillator Type {surge.getParamDisplay(osc_type)} / {surge.getParamVal(osc_type)}")
    for param in osc_contextual_params:
        print(surge.getParamInfo(param))


# Print initial parameters
# As osc type is classic should return Shape, Width 1, Width 2, Sub Mix, Sync, Unison Detune, Unison Voices
print("---Initial Parameters:------------------")
get_osc_params(patch)


# Change osc type to FM3 and refresh conditional params
set_osc_type(patch, 5)
surge.process()
patch = surge.getPatch()

# As osc type is now FM3 should return M1 Amount, M1 Ratio, M2 Amount, M2 Ratio, M3 Amount, M3 Frequency, Feedback
print("---New Parameters:----------------------")
get_osc_params(patch)


# Change osc type to Window and refresh conditional params
set_osc_type(patch, 7)
surge.process()
patch = surge.getPatch()

# As osc type is now Window should return Morph, Formant, Window, Low Cut, High Cut, Unison Detune, Unison Voices
print("---New Parameters:----------------------")
get_osc_params(patch)
