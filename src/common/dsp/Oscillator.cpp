/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2020 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

#include "Oscillator.h"
#include "DSPUtils.h"
#include "FastMath.h"
#include <cmath>

#include "AliasOscillator.h"
#include "AudioInputOscillator.h"
#include "ClassicOscillator.h"
#include "FM2Oscillator.h"
#include "FM3Oscillator.h"
#include "ModernOscillator.h"
#include "SampleAndHoldOscillator.h"
#include "SineOscillator.h"
#include "StringOscillator.h"
#include "TwistOscillator.h"
#include "WavetableOscillator.h"
#include "WindowOscillator.h"

using namespace std;

Oscillator *spawn_osc(int osctype, SurgeStorage *storage, OscillatorStorage *oscdata,
                      pdata *localcopy)
{
    Oscillator *osc = 0;
    switch (osctype)
    {
    case ot_classic:
        return new ClassicOscillator(storage, oscdata, localcopy);
    case ot_wavetable:
        return new WavetableOscillator(storage, oscdata, localcopy);
    case ot_window:
    {
        // In the event we are misconfigured, window oscillator will segfault. If you still play
        // after clicking through 100 warnings, let's just give you a sine
        if (storage && storage->WindowWT.size == 0)
            return new SineOscillator(storage, oscdata, localcopy);

        return new WindowOscillator(storage, oscdata, localcopy);
    }
    case ot_shnoise:
        return new SampleAndHoldOscillator(storage, oscdata, localcopy);
    case ot_audioinput:
        return new AudioInputOscillator(storage, oscdata, localcopy);
    case ot_FM3:
        return new FM3Oscillator(storage, oscdata, localcopy);
    case ot_FM2:
        return new FM2Oscillator(storage, oscdata, localcopy);
    case ot_modern:
        return new ModernOscillator(storage, oscdata, localcopy);
    case ot_string:
        return new StringOscillator(storage, oscdata, localcopy);
    case ot_twist:
        return new TwistOscillator(storage, oscdata, localcopy);
    case ot_alias:
        return new AliasOscillator(storage, oscdata, localcopy);
    case ot_sine:
    default:
        return new SineOscillator(storage, oscdata, localcopy);
    }
    return osc;
}

Oscillator::Oscillator(SurgeStorage *storage, OscillatorStorage *oscdata, pdata *localcopy)
    : master_osc(0)
{
    // assert(storage);
    assert(oscdata);
    this->storage = storage;
    this->oscdata = oscdata;
    this->localcopy = localcopy;
    ticker = 0;
}

Oscillator::~Oscillator() {}
