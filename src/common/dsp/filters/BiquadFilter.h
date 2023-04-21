#pragma once

#include "SurgeStorage.h"
#include "globals.h"
#include "sst/filters/BiquadFilter.h"

// Just use this for this convenient global redefinition to what the class *used* to be
using BiquadFilter = sst::filters::Biquad::BiquadFilter<SurgeStorage, BLOCK_SIZE>;
