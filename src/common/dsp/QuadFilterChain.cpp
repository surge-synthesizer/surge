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
#include "QuadFilterChain.h"
#include "SurgeStorage.h"
#include <vembertech/basic_dsp.h>
#include <vembertech/portable_intrinsics.h>
#include "sst/basic-blocks/mechanics/simd-ops.h"
#include "sst/basic-blocks/dsp/Clippers.h"

namespace mech = sst::basic_blocks::mechanics;
namespace sdsp = sst::basic_blocks::dsp;

#define MWriteOutputs(x)                                                                           \
    d.OutL = SIMD_MM(add_ps)(d.OutL, d.dOutL);                                                     \
    d.OutR = SIMD_MM(add_ps)(d.OutR, d.dOutR);                                                     \
    auto outL = SIMD_MM(mul_ps)(x, d.OutL);                                                        \
    auto outR = SIMD_MM(mul_ps)(x, d.OutR);                                                        \
    SIMD_MM(store_ss)                                                                              \
    (&OutL[k], SIMD_MM(add_ss)(SIMD_MM(load_ss)(&OutL[k]), mech::sum_ps_to_ss(outL)));             \
    SIMD_MM(store_ss)                                                                              \
    (&OutR[k], SIMD_MM(add_ss)(SIMD_MM(load_ss)(&OutR[k]), mech::sum_ps_to_ss(outR)));

#define MWriteOutputsDual(x, y)                                                                    \
    d.OutL = SIMD_MM(add_ps)(d.OutL, d.dOutL);                                                     \
    d.OutR = SIMD_MM(add_ps)(d.OutR, d.dOutR);                                                     \
    d.Out2L = SIMD_MM(add_ps)(d.Out2L, d.dOut2L);                                                  \
    d.Out2R = SIMD_MM(add_ps)(d.Out2R, d.dOut2R);                                                  \
    auto outL = vMAdd(x, d.OutL, vMul(y, d.Out2L));                                                \
    auto outR = vMAdd(x, d.OutR, vMul(y, d.Out2R));                                                \
    SIMD_MM(store_ss)                                                                              \
    (&OutL[k], SIMD_MM(add_ss)(SIMD_MM(load_ss)(&OutL[k]), mech::sum_ps_to_ss(outL)));             \
    SIMD_MM(store_ss)                                                                              \
    (&OutR[k], SIMD_MM(add_ss)(SIMD_MM(load_ss)(&OutR[k]), mech::sum_ps_to_ss(outR)));

#if 0 // DEBUG
#define AssertReasonableAudioFloat(x) assert(x<32.f && x> - 32.f);
#else
#define AssertReasonableAudioFloat(x)
#endif

template <int config, bool A, bool WS, bool B>
void ProcessFBQuad(QuadFilterChainState &d, fbq_global &g, float *OutL, float *OutR)
{
    const auto hb_c = SIMD_MM(set1_ps)(0.5f); // If this is changed from 0.5, make sure to change
                                              // this in the code because it is assumed to be half
    const auto one = SIMD_MM(set1_ps)(1.0f);

    switch (config)
    {
    case fc_serial1: // no feedback at all  (saves CPU)
        for (int k = 0; k < BLOCK_SIZE_OS; k++)
        {
            auto input = d.DL[k];
            auto x = input, y = d.DR[k];
            auto mask = SIMD_MM(load_ps)((float *)&d.FU[0].active);

            if (A)
                x = g.FU1ptr(&d.FU[0], x);
            if (WS)
            {
                d.wsLPF = SIMD_MM(mul_ps)(hb_c, SIMD_MM(add_ps)(d.wsLPF, SIMD_MM(and_ps)(mask, x)));
                d.Drive = SIMD_MM(add_ps)(d.Drive, d.dDrive);
                x = g.WSptr(&d.WSS[0], d.wsLPF, d.Drive);
            }

            if (A || WS)
            {
                d.Mix1 = SIMD_MM(add_ps)(d.Mix1, d.dMix1);
                x = SIMD_MM(add_ps)(SIMD_MM(mul_ps)(input, SIMD_MM(sub_ps)(one, d.Mix1)),
                                    SIMD_MM(mul_ps)(x, d.Mix1));
            }

            y = SIMD_MM(add_ps)(x, y);

            if (B)
                y = g.FU2ptr(&d.FU[1], y);

            d.Mix2 = SIMD_MM(add_ps)(d.Mix2, d.dMix2);
            x = SIMD_MM(add_ps)(SIMD_MM(mul_ps)(x, SIMD_MM(sub_ps)(one, d.Mix2)),
                                SIMD_MM(mul_ps)(y, d.Mix2));
            d.Gain = SIMD_MM(add_ps)(d.Gain, d.dGain);
            auto out = SIMD_MM(and_ps)(mask, SIMD_MM(mul_ps)(x, d.Gain));

            // output stage
            MWriteOutputs(out)
        }
        break;
    case fc_serial2:
        for (int k = 0; k < BLOCK_SIZE_OS; k++)
        {
            d.FB = SIMD_MM(add_ps)(d.FB, d.dFB);
            auto input = vMul(d.FB, d.FBlineL);
            input = vAdd(d.DL[k], sdsp::softclip_ps(input));
            auto mask = SIMD_MM(load_ps)((float *)&d.FU[0].active);
            auto x = input, y = d.DR[k];

            if (A)
                x = g.FU1ptr(&d.FU[0], x);
            if (WS)
            {
                d.wsLPF = SIMD_MM(mul_ps)(hb_c, SIMD_MM(add_ps)(d.wsLPF, SIMD_MM(and_ps)(mask, x)));
                d.Drive = SIMD_MM(add_ps)(d.Drive, d.dDrive);
                x = g.WSptr(&d.WSS[0], d.wsLPF, d.Drive);
            }

            if (A || WS)
            {
                d.Mix1 = SIMD_MM(add_ps)(d.Mix1, d.dMix1);
                x = SIMD_MM(add_ps)(SIMD_MM(mul_ps)(input, SIMD_MM(sub_ps)(one, d.Mix1)),
                                    SIMD_MM(mul_ps)(x, d.Mix1));
            }

            y = SIMD_MM(add_ps)(x, y);

            if (B)
                y = g.FU2ptr(&d.FU[1], y);

            d.Mix2 = SIMD_MM(add_ps)(d.Mix2, d.dMix2);
            x = SIMD_MM(add_ps)(SIMD_MM(mul_ps)(x, SIMD_MM(sub_ps)(one, d.Mix2)),
                                SIMD_MM(mul_ps)(y, d.Mix2));
            d.Gain = SIMD_MM(add_ps)(d.Gain, d.dGain);
            auto out = SIMD_MM(and_ps)(mask, SIMD_MM(mul_ps)(x, d.Gain));
            d.FBlineL = out;

            // output stage
            MWriteOutputs(out)
        }
        break;
    case fc_serial3: // filter 2 is only heard in the feedback path, good for physical modelling
                     // with comb as f2
        for (int k = 0; k < BLOCK_SIZE_OS; k++)
        {
            d.FB = SIMD_MM(add_ps)(d.FB, d.dFB);
            auto input = vMul(d.FB, d.FBlineL);
            input = vAdd(d.DL[k], sdsp::softclip_ps(input));
            auto x = input, y = d.DR[k];
            auto mask = SIMD_MM(load_ps)((float *)&d.FU[0].active);

            if (A)
                x = g.FU1ptr(&d.FU[0], x);
            if (WS)
            {
                d.wsLPF = SIMD_MM(mul_ps)(hb_c, SIMD_MM(add_ps)(d.wsLPF, SIMD_MM(and_ps)(mask, x)));
                d.Drive = SIMD_MM(add_ps)(d.Drive, d.dDrive);
                x = g.WSptr(&d.WSS[0], d.wsLPF, d.Drive);
            }

            if (A || WS)
            {
                d.Mix1 = SIMD_MM(add_ps)(d.Mix1, d.dMix1);
                x = SIMD_MM(add_ps)(SIMD_MM(mul_ps)(input, SIMD_MM(sub_ps)(one, d.Mix1)),
                                    SIMD_MM(mul_ps)(x, d.Mix1));
            }

            // output stage
            d.Gain = SIMD_MM(add_ps)(d.Gain, d.dGain);
            x = SIMD_MM(and_ps)(mask, SIMD_MM(mul_ps)(x, d.Gain));

            MWriteOutputs(x)

                y = SIMD_MM(add_ps)(x, y);

            if (B)
                y = g.FU2ptr(&d.FU[1], y);

            d.Mix2 = SIMD_MM(add_ps)(d.Mix2, d.dMix2);
            x = SIMD_MM(add_ps)(SIMD_MM(mul_ps)(x, SIMD_MM(sub_ps)(one, d.Mix2)),
                                SIMD_MM(mul_ps)(y, d.Mix2));

            d.FBlineL = y;
        }
        break;
    case fc_dual1:
        for (int k = 0; k < BLOCK_SIZE_OS; k++)
        {
            d.FB = SIMD_MM(add_ps)(d.FB, d.dFB);
            auto fb = SIMD_MM(mul_ps)(d.FB, d.FBlineL);
            fb = sdsp::softclip_ps(fb);
            auto x = SIMD_MM(add_ps)(d.DL[k], fb);
            auto y = SIMD_MM(add_ps)(d.DR[k], fb);
            auto mask = SIMD_MM(load_ps)((float *)&d.FU[0].active);

            if (A)
                x = g.FU1ptr(&d.FU[0], x);
            if (B)
                y = g.FU2ptr(&d.FU[1], y);

            d.Mix1 = SIMD_MM(add_ps)(d.Mix1, d.dMix1);
            d.Mix2 = SIMD_MM(add_ps)(d.Mix2, d.dMix2);
            x = SIMD_MM(add_ps)(SIMD_MM(mul_ps)(x, d.Mix1), SIMD_MM(mul_ps)(y, d.Mix2));

            if (WS)
            {
                d.wsLPF = SIMD_MM(mul_ps)(hb_c, SIMD_MM(add_ps)(d.wsLPF, SIMD_MM(and_ps)(mask, x)));
                d.Drive = SIMD_MM(add_ps)(d.Drive, d.dDrive);
                x = g.WSptr(&d.WSS[0], d.wsLPF, d.Drive);
            }

            d.Gain = SIMD_MM(add_ps)(d.Gain, d.dGain);
            auto out = SIMD_MM(and_ps)(mask, SIMD_MM(mul_ps)(x, d.Gain));
            d.FBlineL = out;
            // output stage
            MWriteOutputs(out)
        }
        break;
    case fc_dual2:
        for (int k = 0; k < BLOCK_SIZE_OS; k++)
        {
            d.FB = SIMD_MM(add_ps)(d.FB, d.dFB);
            auto fb = SIMD_MM(mul_ps)(d.FB, d.FBlineL);
            fb = sdsp::softclip_ps(fb);
            auto x = SIMD_MM(add_ps)(d.DL[k], fb);
            auto y = SIMD_MM(add_ps)(d.DR[k], fb);
            auto mask = SIMD_MM(load_ps)((float *)&d.FU[0].active);

            if (A)
                x = g.FU1ptr(&d.FU[0], x);
            if (WS)
            {
                d.wsLPF = SIMD_MM(mul_ps)(hb_c, SIMD_MM(add_ps)(d.wsLPF, SIMD_MM(and_ps)(mask, x)));
                d.Drive = SIMD_MM(add_ps)(d.Drive, d.dDrive);
                x = g.WSptr(&d.WSS[0], d.wsLPF, d.Drive);
            }

            if (B)
                y = g.FU2ptr(&d.FU[1], y);

            d.Mix1 = SIMD_MM(add_ps)(d.Mix1, d.dMix1);
            d.Mix2 = SIMD_MM(add_ps)(d.Mix2, d.dMix2);
            x = SIMD_MM(add_ps)(SIMD_MM(mul_ps)(x, d.Mix1), SIMD_MM(mul_ps)(y, d.Mix2));

            d.Gain = SIMD_MM(add_ps)(d.Gain, d.dGain);
            auto out = SIMD_MM(and_ps)(mask, SIMD_MM(mul_ps)(x, d.Gain));
            d.FBlineL = out;
            // output stage
            MWriteOutputs(out)
        }
        break;
    case fc_ring:
        for (int k = 0; k < BLOCK_SIZE_OS; k++)
        {
            d.FB = SIMD_MM(add_ps)(d.FB, d.dFB);
            auto fb = SIMD_MM(mul_ps)(d.FB, d.FBlineL);
            fb = sdsp::softclip_ps(fb);
            auto x = SIMD_MM(add_ps)(d.DL[k], fb);
            auto y = SIMD_MM(add_ps)(d.DR[k], fb);
            auto mask = SIMD_MM(load_ps)((float *)&d.FU[0].active);

            if (A)
                x = g.FU1ptr(&d.FU[0], x);
            if (B)
                y = g.FU2ptr(&d.FU[1], y);

            d.Mix1 = SIMD_MM(add_ps)(d.Mix1, d.dMix1);
            d.Mix2 = SIMD_MM(add_ps)(d.Mix2, d.dMix2);

            x = SIMD_MM(mul_ps)(SIMD_MM(add_ps)(SIMD_MM(mul_ps)(SIMD_MM(sub_ps)(one, d.Mix1), y),
                                                SIMD_MM(mul_ps)(x, d.Mix1)),
                                SIMD_MM(add_ps)(SIMD_MM(mul_ps)(SIMD_MM(sub_ps)(one, d.Mix2), x),
                                                SIMD_MM(mul_ps)(y, d.Mix2)));

            if (WS)
            {
                d.wsLPF = SIMD_MM(mul_ps)(hb_c, SIMD_MM(add_ps)(d.wsLPF, x));
                d.Drive = SIMD_MM(add_ps)(d.Drive, d.dDrive);
                x = g.WSptr(&d.WSS[0], SIMD_MM(and_ps)(mask, d.wsLPF), d.Drive);
            }

            d.Gain = SIMD_MM(add_ps)(d.Gain, d.dGain);
            auto out = SIMD_MM(and_ps)(mask, SIMD_MM(mul_ps)(x, d.Gain));
            d.FBlineL = out;
            // output stage
            MWriteOutputs(out)
        }
        break;
    case fc_stereo:
        for (int k = 0; k < BLOCK_SIZE_OS; k++)
        {
            d.FB = SIMD_MM(add_ps)(d.FB, d.dFB);
            auto fb = SIMD_MM(mul_ps)(d.FB, d.FBlineL);
            fb = sdsp::softclip_ps(fb);
            auto x = SIMD_MM(add_ps)(d.DL[k], fb);
            auto y = SIMD_MM(add_ps)(d.DR[k], fb);
            auto mask = SIMD_MM(load_ps)((float *)&d.FU[0].active);

            if (A)
                x = g.FU1ptr(&d.FU[0], x);
            if (B)
                y = g.FU2ptr(&d.FU[1], y);

            if (WS)
            {
                d.Drive = SIMD_MM(add_ps)(d.Drive, d.dDrive);
                x = g.WSptr(&d.WSS[0], SIMD_MM(and_ps)(mask, x), d.Drive);
                y = g.WSptr(&d.WSS[1], SIMD_MM(and_ps)(mask, y), d.Drive);
            }

            d.Mix1 = SIMD_MM(add_ps)(d.Mix1, d.dMix1);
            d.Mix2 = SIMD_MM(add_ps)(d.Mix2, d.dMix2);
            x = SIMD_MM(mul_ps)(x, d.Mix1);
            y = SIMD_MM(mul_ps)(y, d.Mix2);

            d.Gain = SIMD_MM(add_ps)(d.Gain, d.dGain);
            x = SIMD_MM(and_ps)(mask, SIMD_MM(mul_ps)(x, d.Gain));
            y = SIMD_MM(and_ps)(mask, SIMD_MM(mul_ps)(y, d.Gain));
            d.FBlineL = SIMD_MM(add_ps)(x, y);

            // output stage
            MWriteOutputsDual(x, y) AssertReasonableAudioFloat(OutL[k]);
            AssertReasonableAudioFloat(OutR[k]);
        }
        break;
    case fc_wide:
        for (int k = 0; k < BLOCK_SIZE_OS; k++)
        {
            d.FB = SIMD_MM(add_ps)(d.FB, d.dFB);
            auto fbL = SIMD_MM(mul_ps)(d.FB, d.FBlineL);
            auto fbR = SIMD_MM(mul_ps)(d.FB, d.FBlineR);
            auto xin = SIMD_MM(add_ps)(d.DL[k], sdsp::softclip_ps(fbL));
            auto yin = SIMD_MM(add_ps)(d.DR[k], sdsp::softclip_ps(fbR));
            auto x = xin;
            auto y = yin;

            auto mask = SIMD_MM(load_ps)((float *)&d.FU[0].active);

            if (A)
            {
                x = g.FU1ptr(&d.FU[0], x);
                y = g.FU1ptr(&d.FU[2], y);
            }

            if (WS)
            {
                d.Drive = SIMD_MM(add_ps)(d.Drive, d.dDrive);
                x = g.WSptr(&d.WSS[0], SIMD_MM(and_ps)(mask, x), d.Drive);
                y = g.WSptr(&d.WSS[1], SIMD_MM(and_ps)(mask, y), d.Drive);
            }

            if (A || WS)
            {
                d.Mix1 = SIMD_MM(add_ps)(d.Mix1, d.dMix1);
                auto t = SIMD_MM(sub_ps)(one, d.Mix1);
                x = SIMD_MM(add_ps)(SIMD_MM(mul_ps)(xin, t), SIMD_MM(mul_ps)(x, d.Mix1));
                y = SIMD_MM(add_ps)(SIMD_MM(mul_ps)(yin, t), SIMD_MM(mul_ps)(y, d.Mix1));
            }

            if (B)
            {
                auto z = g.FU2ptr(&d.FU[1], x);
                auto w = g.FU2ptr(&d.FU[3], y);

                d.Mix2 = SIMD_MM(add_ps)(d.Mix2, d.dMix2);
                auto t = SIMD_MM(sub_ps)(one, d.Mix2);
                x = SIMD_MM(add_ps)(SIMD_MM(mul_ps)(x, t), SIMD_MM(mul_ps)(z, d.Mix2));
                y = SIMD_MM(add_ps)(SIMD_MM(mul_ps)(y, t), SIMD_MM(mul_ps)(w, d.Mix2));
            }

            d.Gain = SIMD_MM(add_ps)(d.Gain, d.dGain);
            x = SIMD_MM(and_ps)(mask, SIMD_MM(mul_ps)(x, d.Gain));
            y = SIMD_MM(and_ps)(mask, SIMD_MM(mul_ps)(y, d.Gain));
            d.FBlineL = x;
            d.FBlineR = y;

            // output stage
            MWriteOutputsDual(x, y) AssertReasonableAudioFloat(OutL[k]);
            AssertReasonableAudioFloat(OutR[k]);
        }
        break;
    }
}

template <int config> FBQFPtr GetFBQPointer2(bool A, bool WS, bool B)
{
    if (A)
    {
        if (B)
        {
            if (WS)
                return ProcessFBQuad<config, 1, 1, 1>;
            else
                return ProcessFBQuad<config, 1, 0, 1>;
        }
        else
        {
            if (WS)
                return ProcessFBQuad<config, 1, 1, 0>;
            else
                return ProcessFBQuad<config, 1, 0, 0>;
        }
    }
    else
    {
        if (B)
        {
            if (WS)
                return ProcessFBQuad<config, 0, 1, 1>;
            else
                return ProcessFBQuad<config, 0, 0, 1>;
        }
        else
        {
            if (WS)
                return ProcessFBQuad<config, 0, 1, 0>;
            else
                return ProcessFBQuad<config, 0, 0, 0>;
        }
    }
    return 0;
}

FBQFPtr GetFBQPointer(int config, bool A, bool WS, bool B)
{
    switch (config)
    {
    case fc_serial1:
        return GetFBQPointer2<fc_serial1>(A, WS, B);
    case fc_serial2:
        return GetFBQPointer2<fc_serial2>(A, WS, B);
    case fc_serial3:
        return GetFBQPointer2<fc_serial3>(A, WS, B);
    case fc_dual1:
        return GetFBQPointer2<fc_dual1>(A, WS, B);
    case fc_dual2:
        return GetFBQPointer2<fc_dual2>(A, WS, B);
    case fc_ring:
        return GetFBQPointer2<fc_ring>(A, WS, B);
    case fc_stereo:
        return GetFBQPointer2<fc_stereo>(A, WS, B);
    case fc_wide:
        return GetFBQPointer2<fc_wide>(A, WS, B);
    }
    return 0;
}

void InitQuadFilterChainStateToZero(QuadFilterChainState *Q)
{
    Q->Gain = SIMD_MM(setzero_ps)();
    Q->FB = SIMD_MM(setzero_ps)();
    Q->Mix1 = SIMD_MM(setzero_ps)();
    Q->Mix2 = SIMD_MM(setzero_ps)();
    Q->Drive = SIMD_MM(setzero_ps)();
    Q->dGain = SIMD_MM(setzero_ps)();
    Q->dFB = SIMD_MM(setzero_ps)();
    Q->dMix1 = SIMD_MM(setzero_ps)();
    Q->dMix2 = SIMD_MM(setzero_ps)();
    Q->dDrive = SIMD_MM(setzero_ps)();

    Q->wsLPF = SIMD_MM(setzero_ps)();
    Q->FBlineL = SIMD_MM(setzero_ps)();
    Q->FBlineR = SIMD_MM(setzero_ps)();

    for (auto i = 0; i < BLOCK_SIZE_OS; ++i)
    {
        Q->DL[i] = SIMD_MM(setzero_ps)();
        Q->DR[i] = SIMD_MM(setzero_ps)();
    }

    Q->OutL = SIMD_MM(setzero_ps)();
    Q->OutR = SIMD_MM(setzero_ps)();
    Q->dOutL = SIMD_MM(setzero_ps)();
    Q->dOutR = SIMD_MM(setzero_ps)();
    Q->Out2L = SIMD_MM(setzero_ps)();
    Q->Out2R = SIMD_MM(setzero_ps)();
    Q->dOut2L = SIMD_MM(setzero_ps)();
    Q->dOut2R = SIMD_MM(setzero_ps)();
}
