#pragma once

#include "SurgeStorage.h"
#include "DSPUtils.h"
#include <vembertech/lipol.h>
#include "BiquadFilter.h"

#include "OscillatorBase.h"

Oscillator *spawn_osc(int osctype, SurgeStorage *storage, OscillatorStorage *oscdata,
                      pdata *localcopy);
