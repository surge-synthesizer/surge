#pragma once

#include "SurgeStorage.h"
#include "DSPUtils.h"
#include <vembertech/lipol.h>
#include "BiquadFilter.h"

#include "OscillatorBase.h"

static constexpr size_t oscillator_buffer_size = 16 * 1024;

Oscillator *spawn_osc(int osctype, SurgeStorage *storage, OscillatorStorage *oscdata,
                      pdata *localcopy,
                      unsigned char *onto); // This buffer should be at least oscillator_buffer_size
