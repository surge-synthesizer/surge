#pragma once

#include "SurgeStorage.h"
#include "DspUtilities.h"
#include <vt_dsp/lipol.h>
#include "BiquadFilter.h"

#include "OscillatorBase.h"

Oscillator *spawn_osc(int osctype, SurgeStorage *storage, OscillatorStorage *oscdata,
                      pdata *localcopy);
