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
#include <utility>

using namespace std;

Oscillator *spawn_osc(int osctype, SurgeStorage *storage, OscillatorStorage *oscdata,
                      pdata *localcopy, unsigned char *onto)
{
    static bool checkSizes = true;
    if (checkSizes)
    {
        size_t mx = 0;
// #define S(x) std::cout << "sizeof(" << #x << ")=" << sizeof(x) << std::endl; mx = std::max(mx,
// sizeof(x));
#define S(x) mx = std::max(mx, sizeof(x));
        S(ClassicOscillator);
        S(WavetableOscillator);
        S(WindowOscillator);
        S(SineOscillator);
        S(SampleAndHoldOscillator);
        S(AudioInputOscillator);
        S(FM2Oscillator);
        S(FM3Oscillator);
        S(ModernOscillator);
        S(StringOscillator);
        S(TwistOscillator);
        S(AliasOscillator);

        checkSizes = false;

        if (mx > oscillator_buffer_size)
        {
            /*
             * If you hit this you have added members to an oscillator type which are bigger than
             * the static buffer into which we allocate oscillators. Basically at this point you
             * either want to make your class smaller, make the buffer size bigger, or grab one of
             * the other devs and ask for help.
             *
             * Surge will be so horribly broken in this case and it is a compile time condition that
             * I decided to just throw an error out of here and crash everything. No way you can
             * trigger this without changing source so it is safe!
             */
            std::cout
                << "PANIC: Oscillator Buffer Size smaller than max sizeof one oscillator class"
                << std::endl;
            throw std::runtime_error("Software Error - Resize class or buffer");
        }
    }

    Oscillator *osc = 0;
    switch (osctype)
    {
    case ot_classic:
        return new (onto) ClassicOscillator(storage, oscdata, localcopy);
    case ot_wavetable:
        return new (onto) WavetableOscillator(storage, oscdata, localcopy);
    case ot_window:
    {
        // In the event we are misconfigured, window oscillator will segfault. If you still play
        // after clicking through 100 warnings, let's just give you a sine
        if (storage && storage->WindowWT.size == 0)
            return new (onto) SineOscillator(storage, oscdata, localcopy);

        return new (onto) WindowOscillator(storage, oscdata, localcopy);
    }
    case ot_shnoise:
        return new (onto) SampleAndHoldOscillator(storage, oscdata, localcopy);
    case ot_audioinput:
        return new (onto) AudioInputOscillator(storage, oscdata, localcopy);
    case ot_FM3:
        return new (onto) FM3Oscillator(storage, oscdata, localcopy);
    case ot_FM2:
        return new (onto) FM2Oscillator(storage, oscdata, localcopy);
    case ot_modern:
        return new (onto) ModernOscillator(storage, oscdata, localcopy);
    case ot_string:
        return new (onto) StringOscillator(storage, oscdata, localcopy);
    case ot_twist:
        return new (onto) TwistOscillator(storage, oscdata, localcopy);
    case ot_alias:
        return new (onto) AliasOscillator(storage, oscdata, localcopy);
    case ot_sine:
    default:
        return new (onto) SineOscillator(storage, oscdata, localcopy);
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
