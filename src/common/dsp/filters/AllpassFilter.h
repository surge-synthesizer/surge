/*
 * Surge XT - a free and open source hybrid synthesizer,
 * built by Surge Synth Team
 *
 * Learn more at https://surge-synthesizer.github.io/
 *
 * Copyright 2018-2024, various authors, as described in the GitHub
 * transaction log.
 *
 * Surge XT is released under the GNU General Public Licence v3
 * or later (GPL-3.0-or-later). The license is found in the "LICENSE"
 * file in the root of this repository, or at
 * https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 * Surge was a commercial product from 2004-2018, copyright and ownership
 * held by Claes Johanson at Vember Audio during that period.
 * Claes made Surge open source in September 2018.
 *
 * All source for Surge XT is available at
 * https://github.com/surge-synthesizer/surge
 */
#ifndef SURGE_SRC_COMMON_DSP_FILTERS_ALLPASSFILTER_H
#define SURGE_SRC_COMMON_DSP_FILTERS_ALLPASSFILTER_H

template <int dt> class AllpassFilter
{
  public:
    AllpassFilter()
    {
        wpos = 0;
        setA(0.3);
        for (int i = 0; i < dt; i++)
            buffer[i] = 0.f;
    }
    float process(float x)
    {
        wpos = (wpos + 1) % dt;
        float y = buffer[wpos];
        buffer[wpos] = y * -a + x;
        return y + buffer[wpos] * a;
    }
    void setA(float a) { this->a = a; }

  protected:
    float buffer[dt];
    float a;
    int wpos;
};

#endif // SURGE_SRC_COMMON_DSP_FILTERS_ALLPASSFILTER_H
