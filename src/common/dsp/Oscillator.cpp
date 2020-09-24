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
#include "DspUtilities.h"
#include "FastMath.h"
#include <cmath>

using namespace std;

#include "AudioInputOscillator.h"
#include "FM2Oscillator.h"
#include "FM3Oscillator.h"
#include "SampleAndHoldOscillator.h"
#include "SinOscillator.h"
#include "SurgeSuperOscillator.h"
#include "WavetableOscillator.h"
#include "WindowOscillator.h"

Oscillator*
spawn_osc(int osctype, SurgeStorage* storage, OscillatorStorage* oscdata, pdata* localcopy)
{
   Oscillator* osc = 0;
   switch (osctype)
   {
   case ot_classic:
      return new SurgeSuperOscillator(storage, oscdata, localcopy);
   case ot_wavetable:
      return new WavetableOscillator(storage, oscdata, localcopy);
   case ot_WT2:
   {
      // In the event we are misconfigured, window will segv. If you still play
      // after clicking through 100 warnings, lets jsut give youa sin
      if( storage && storage->WindowWT.size == 0 )
         return new SinOscillator( storage, oscdata, localcopy );
 
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
   case ot_sinus:
   default:
      return new SinOscillator(storage, oscdata, localcopy);
   }
   return osc;
}

Oscillator::Oscillator(SurgeStorage* storage, OscillatorStorage* oscdata, pdata* localcopy)
    : master_osc(0)
{
   // assert(storage);
   assert(oscdata);
   this->storage = storage;
   this->oscdata = oscdata;
   this->localcopy = localcopy;
   ticker = 0;
}

Oscillator::~Oscillator()
{}

