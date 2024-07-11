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

#include "WindowOscillator.h"
#include "DSPUtils.h"

#include <cstdint>
#include "DebugHelpers.h"

#ifdef _WIN32
#include <intrin.h>
#else
namespace
{
inline bool _BitScanReverse(unsigned long *result, unsigned long bits)
{
    *result = __builtin_ctz(bits);
    return true;
}
} // anonymous namespace
#endif
int Float2Int(float x)
{
#ifdef ARM_NEON
    return int(x + 0.5f);
#else
    return _mm_cvt_ss2si(_mm_load_ss(&x));
#endif
}
WindowOscillator::WindowOscillator(SurgeStorage *storage, OscillatorStorage *oscdata,
                                   pdata *localcopy)
    : Oscillator(storage, oscdata, localcopy), lp(storage), hp(storage)
{
}

template <bool is_init> void WindowOscillator::update_lagvals()
{
    l_morph.newValue(limit_range(localcopy[oscdata->p[win_morph].param_id_in_scene].f, 0.f, 1.f));

    if (is_init)
    {
        l_morph.instantize();
    }
}

void WindowOscillator::init(float pitch, bool is_display, bool nonzero_init_drift)
{
    memset(&Window, 0, sizeof(Window));

    l_morph.setRate(0.05);
    update_lagvals<true>();

    NumUnison = limit_range(oscdata->p[win_unison_voices].val.i, 1, MAX_UNISON - 1);

    if (is_display)
    {
        NumUnison = 1;
    }

    float out_attenuation_inv = sqrt((float)NumUnison);
    OutAttenuation = 1.0f / (out_attenuation_inv * 16777216.f);

    if (NumUnison == 1)
    {
        DetuneBias = 1;
        DetuneOffset = 0;

        Window.Gain[0][0] = 128;
        Window.Gain[0][1] = 128; // unity gain

        if (oscdata->retrigger.val.b || is_display)
        {
            Window.Pos[0] = (storage->WindowWT.size + storage->WindowWT.size) << 16;
        }
        else
        {
            Window.Pos[0] =
                (storage->WindowWT.size + (storage->rand() & (storage->WindowWT.size - 1))) << 16;
        }

        Window.driftLFO[0].init(nonzero_init_drift);
    }
    else
    {
        DetuneBias = (float)2.f / ((float)NumUnison - 1.f);
        DetuneOffset = -1.f;

        bool odd = NumUnison & 1;
        float mid = NumUnison * 0.5 - 0.5;
        int half = NumUnison >> 1;

        for (int i = 0; i < NumUnison; i++)
        {
            float d = fabs((float)i - mid) / mid;

            if (odd && (i >= half))
                d = -d;
            if (i & 1)
                d = -d;

            Window.Gain[i][0] = limit_range((int)(float)(128.f * megapanL(d)), 0, 255);
            Window.Gain[i][1] = limit_range((int)(float)(128.f * megapanR(d)), 0, 255);

            if (oscdata->retrigger.val.b)
            {
                Window.Pos[i] =
                    (storage->WindowWT.size + ((storage->WindowWT.size * i) / NumUnison)) << 16;
            }
            else
            {
                Window.Pos[i] =
                    (storage->WindowWT.size + (storage->rand() & (storage->WindowWT.size - 1)))
                    << 16;
            }

            Window.driftLFO[i].init(true); // Window has always started uni voices with non zero
        }
    }

    hp.coeff_instantize();
    lp.coeff_instantize();

    hp.coeff_HP(hp.calc_omega(oscdata->p[win_lowcut].val.f / 12.0) / OSC_OVERSAMPLING, 0.707);
    lp.coeff_LP2B(lp.calc_omega(oscdata->p[win_highcut].val.f / 12.0) / OSC_OVERSAMPLING, 0.707);
}

WindowOscillator::~WindowOscillator() {}

void WindowOscillator::init_ctrltypes()
{
    oscdata->p[win_morph].set_name("Morph");
    oscdata->p[win_morph].set_type(ct_countedset_percent_extendable);
    oscdata->p[win_morph].set_user_data(oscdata);

    oscdata->p[win_formant].set_name("Formant");
    oscdata->p[win_formant].set_type(ct_pitch);
    oscdata->p[win_window].set_name("Window");
    oscdata->p[win_window].set_type(ct_wt2window);

    oscdata->p[win_lowcut].set_name("Low Cut");
    oscdata->p[win_lowcut].set_type(ct_freq_audible_deactivatable_hp);
    oscdata->p[win_highcut].set_name("High Cut");
    oscdata->p[win_highcut].set_type(ct_freq_audible_deactivatable_lp);

    oscdata->p[win_unison_detune].set_name("Unison Detune");
    oscdata->p[win_unison_detune].set_type(ct_oscspread);
    oscdata->p[win_unison_voices].set_name("Unison Voices");
    oscdata->p[win_unison_voices].set_type(ct_osccount);
}

void WindowOscillator::init_default_values()
{
    oscdata->p[win_morph].val.f = 0.0f;
    oscdata->p[win_formant].val.f = 0.0f;
    oscdata->p[win_window].val.i = 0;

    // low cut at the bottom
    oscdata->p[win_lowcut].val.f = oscdata->p[win_lowcut].val_min.f;
    oscdata->p[win_lowcut].deactivated = true;

    // high cut at the top
    oscdata->p[win_highcut].val.f = oscdata->p[win_highcut].val_max.f;
    oscdata->p[win_highcut].deactivated = true;

    oscdata->p[win_unison_detune].val.f = 0.1f;
    oscdata->p[win_unison_voices].val.i = 1;
}

inline unsigned int BigMULr16(unsigned int a, unsigned int b)
{
    // 64-bit unsigned multiply with right shift by 16 bits
#ifdef _MSC_VER
    const auto c{__emulu(a, b)};
#else
    const auto c{std::uint64_t{a} * std::uint64_t{b}};
#endif
    return c >> 16u;
}

template <bool FM, bool Full16> void WindowOscillator::ProcessWindowOscs(bool stereo)
{
    const unsigned int M0Mask = 0x07f8;
    unsigned int SizeMask = (oscdata->wt.size << 16) - 1;
    unsigned int SizeMaskWin = (storage->WindowWT.size << 16) - 1;

    unsigned char SelWindow = limit_range(oscdata->p[win_window].val.i, 0, 8);

    int Table = limit_range((int)(float)(oscdata->wt.n_tables * l_morph.v), 0,
                            (int)oscdata->wt.n_tables - 1);
    int TablePlusOne = limit_range(Table + 1, 0, (int)oscdata->wt.n_tables - 1);
    float frac = oscdata->wt.n_tables * l_morph.v;
    float FTable = limit_range(frac - Table, 0.f, 1.f);

    if (!oscdata->p[win_morph].extend_range)
    {
        // If I'm not extended, then clamp to table
        FTable = 0.f;
    }

    int FormantMul =
        (int)(float)(65536.f * storage->note_to_pitch_tuningctr(
                                   localcopy[oscdata->p[win_formant].param_id_in_scene].f));

    // We can actually get input tables bigger than the convolution table
    int WindowVsWavePO2 = storage->WindowWT.size_po2 - oscdata->wt.size_po2;

    if (WindowVsWavePO2 < 0)
    {
        FormantMul = std::max(FormantMul << -WindowVsWavePO2, 1);
    }
    else
    {
        FormantMul = std::max(FormantMul >> WindowVsWavePO2, 1);
    }

    {
        // SSE2 path
        for (int so = 0; so < NumUnison; so++)
        {
            unsigned int Pos = Window.Pos[so];
            unsigned int RatioA = Window.Ratio[so];

            if (FM)
                RatioA = Window.FMRatio[so][0];

            unsigned int MipMapA = 0;
            unsigned int MipMapB = 0;

            if (Window.Table[0][so] >= oscdata->wt.n_tables || oscdata->p[win_morph].extend_range)
            {
                Window.Table[0][so] = Table;
            }

            if (Window.Table[1][so] >= oscdata->wt.n_tables || oscdata->p[win_morph].extend_range)
            {
                Window.Table[1][so] = TablePlusOne;
            }

            unsigned long MSBpos;
            unsigned int bs = BigMULr16(RatioA, 3 * FormantMul);

            if (_BitScanReverse(&MSBpos, bs))
                MipMapB = limit_range((int)MSBpos - 17, 0, oscdata->wt.size_po2 - 1);

            if (_BitScanReverse(&MSBpos, 3 * RatioA))
                MipMapA = limit_range((int)MSBpos - 17, 0, storage->WindowWT.size_po2 - 1);

            short *WaveAdr = oscdata->wt.TableI16WeakPointers[MipMapB][Window.Table[0][so]];
            short *WaveAdrP1 = oscdata->wt.TableI16WeakPointers[MipMapB][Window.Table[1][so]];
            short *WinAdr = storage->WindowWT.TableI16WeakPointers[MipMapA][SelWindow];

            for (int i = 0; i < BLOCK_SIZE_OS; i++)
            {
                if (FM)
                {
                    Pos += Window.FMRatio[so][i];
                }
                else
                {
                    Pos += RatioA;
                }

                if (Pos & ~SizeMaskWin)
                {
                    Window.FormantMul[so] = FormantMul;
                    Window.Table[0][so] = Table;
                    Window.Table[1][so] = TablePlusOne;
                    WaveAdr = oscdata->wt.TableI16WeakPointers[MipMapB][Table];
                    WaveAdrP1 = oscdata->wt.TableI16WeakPointers[MipMapB][TablePlusOne];
                    Pos = Pos & SizeMaskWin;
                }

                unsigned int WinPos = Pos >> (16 + MipMapA);
                unsigned int WinSPos = (Pos >> (8 + MipMapA)) & 0xFF;

                unsigned int FPos = BigMULr16(Window.FormantMul[so], Pos) & SizeMask;

                unsigned int MPos = FPos >> (16 + MipMapB);
                unsigned int MSPos = ((FPos >> (8 + MipMapB)) & 0xFF);

                __m128i Wave =
                    _mm_madd_epi16(_mm_load_si128(((__m128i *)storage->sinctableI16 + MSPos)),
                                   _mm_loadu_si128((__m128i *)&WaveAdr[MPos]));
                __m128i WaveP1 =
                    _mm_madd_epi16(_mm_load_si128(((__m128i *)storage->sinctableI16 + MSPos)),
                                   _mm_loadu_si128((__m128i *)&WaveAdrP1[MPos]));

                __m128i Win =
                    _mm_madd_epi16(_mm_load_si128(((__m128i *)storage->sinctableI16 + WinSPos)),
                                   _mm_loadu_si128((__m128i *)&WinAdr[WinPos]));

                // Sum
                int iWin alignas(16)[4], iWaveP1 alignas(16)[4], iWave alignas(16)[4];
#if MAC
                // this should be very fast on C2D/C1D (and there are no macs with K8's)
                iWin[0] = _mm_cvtsi128_si32(Win);
                iWin[1] = _mm_cvtsi128_si32(_mm_shuffle_epi32(Win, _MM_SHUFFLE(1, 1, 1, 1)));
                iWin[2] = _mm_cvtsi128_si32(_mm_shuffle_epi32(Win, _MM_SHUFFLE(2, 2, 2, 2)));
                iWin[3] = _mm_cvtsi128_si32(_mm_shuffle_epi32(Win, _MM_SHUFFLE(3, 3, 3, 3)));
                iWave[0] = _mm_cvtsi128_si32(Wave);
                iWave[1] = _mm_cvtsi128_si32(_mm_shuffle_epi32(Wave, _MM_SHUFFLE(1, 1, 1, 1)));
                iWave[2] = _mm_cvtsi128_si32(_mm_shuffle_epi32(Wave, _MM_SHUFFLE(2, 2, 2, 2)));
                iWave[3] = _mm_cvtsi128_si32(_mm_shuffle_epi32(Wave, _MM_SHUFFLE(3, 3, 3, 3)));
                iWaveP1[0] = _mm_cvtsi128_si32(WaveP1);
                iWaveP1[1] = _mm_cvtsi128_si32(_mm_shuffle_epi32(WaveP1, _MM_SHUFFLE(1, 1, 1, 1)));
                iWaveP1[2] = _mm_cvtsi128_si32(_mm_shuffle_epi32(WaveP1, _MM_SHUFFLE(2, 2, 2, 2)));
                iWaveP1[3] = _mm_cvtsi128_si32(_mm_shuffle_epi32(WaveP1, _MM_SHUFFLE(3, 3, 3, 3)));

#else
                _mm_store_si128((__m128i *)&iWin, Win);
                _mm_store_si128((__m128i *)&iWave, Wave);
                _mm_store_si128((__m128i *)&iWaveP1, WaveP1);
#endif

                iWin[0] = (iWin[0] + iWin[1] + iWin[2] + iWin[3]) >> 13;
                iWave[0] = (iWave[0] + iWave[1] + iWave[2] + iWave[3]) >> (13 + (Full16 ? 1 : 0));
                iWaveP1[0] =
                    (iWaveP1[0] + iWaveP1[1] + iWaveP1[2] + iWaveP1[3]) >> (13 + (Full16 ? 1 : 0));

                iWave[0] = (int)((1.f - FTable) * iWave[0] + FTable * iWaveP1[0]);

                if (stereo)
                {
                    int Out = (iWin[0] * iWave[0]) >> 7;
                    IOutputL[i] += (Out * (int)Window.Gain[so][0]) >> 6;
                    IOutputR[i] += (Out * (int)Window.Gain[so][1]) >> 6;
                }
                else
                    IOutputL[i] += (iWin[0] * iWave[0]) >> 6;
            }

            Window.Pos[so] = Pos;
        }
    }
}

void WindowOscillator::process_block(float pitch, float drift, bool stereo, bool FM, float fmdepth)
{
    memset(IOutputL, 0, BLOCK_SIZE_OS * sizeof(int));

    if (stereo)
    {
        memset(IOutputR, 0, BLOCK_SIZE_OS * sizeof(int));
    }

    update_lagvals<false>();
    l_morph.process();

    float Detune;

    if (oscdata->p[win_unison_detune].absolute)
    {
        // See comment in ClassicOscillator
        Detune = oscdata->p[win_unison_detune].get_extended(
                     localcopy[oscdata->p[win_unison_detune].param_id_in_scene].f) *
                 storage->note_to_pitch_inv_ignoring_tuning(std::min(148.f, pitch)) * 16 / 0.9443;
    }
    else
    {
        Detune = oscdata->p[win_unison_detune].get_extended(
            localcopy[oscdata->p[win_unison_detune].param_id_in_scene].f);
    }

    float fmstrength = 32 * M_PI * fmdepth * fmdepth * fmdepth;

    for (int l = 0; l < NumUnison; l++)
    {
        Window.driftLFO[l].next();
        /*
        ** This original code uses note 57 as a center point with a frequency of 220.
        */

        float f = storage->note_to_pitch(pitch + drift * Window.driftLFO[l].val() +
                                         Detune * (DetuneOffset + DetuneBias * (float)l));
        int Ratio = Float2Int(8.175798915f * 32768.f * f * (float)(storage->WindowWT.size) *
                              storage->samplerate_inv); // (65536.f*0.5f), 0.5 for oversampling

        Window.Ratio[l] = Ratio;

        if (FM)
        {
            FMdepth[l].newValue(fmstrength);

            for (int i = 0; i < BLOCK_SIZE_OS; ++i)
            {
                float fmadj = (1.0 + FMdepth[l].v * master_osc[i]);
                float f = storage->note_to_pitch(pitch + drift * Window.driftLFO[l].val() +
                                                 Detune * (DetuneOffset + DetuneBias * (float)l));
                int Ratio =
                    Float2Int(8.175798915f * 32768.f * f * fmadj * (float)(storage->WindowWT.size) *
                              storage->samplerate_inv); // (65536.f*0.5f), 0.5 for oversampling

                Window.FMRatio[l][i] = Ratio;
                FMdepth[l].process();
            }
        }
    }

    bool is16 = oscdata->wt.flags & wtf_int16_is_16;

    if (FM)
    {
        if (is16)
        {
            ProcessWindowOscs<true, true>(stereo);
        }
        else
        {
            ProcessWindowOscs<true, false>(stereo);
        }
    }
    else
    {
        if (is16)
        {
            ProcessWindowOscs<false, true>(stereo);
        }
        else
        {
            ProcessWindowOscs<false, false>(stereo);
        }
    }

    // int32 -> float conversion
    __m128 scale = _mm_load1_ps(&OutAttenuation);
    {
        // SSE2 path
        if (stereo)
        {
            for (int i = 0; i < BLOCK_SIZE_OS; i += 4)
            {
                _mm_store_ps(&output[i],
                             _mm_mul_ps(_mm_cvtepi32_ps(*(__m128i *)&IOutputL[i]), scale));
                _mm_store_ps(&outputR[i],
                             _mm_mul_ps(_mm_cvtepi32_ps(*(__m128i *)&IOutputR[i]), scale));
            }
        }
        else
        {
            for (int i = 0; i < BLOCK_SIZE_OS; i += 4)
            {
                _mm_store_ps(&output[i],
                             _mm_mul_ps(_mm_cvtepi32_ps(*(__m128i *)&IOutputL[i]), scale));
            }
        }
    }

    applyFilter();
}

void WindowOscillator::applyFilter()
{
    if (!oscdata->p[win_lowcut].deactivated)
    {
        auto par = &(oscdata->p[win_lowcut]);
        auto pv = limit_range(localcopy[par->param_id_in_scene].f, par->val_min.f, par->val_max.f);
        hp.coeff_HP(hp.calc_omega(pv / 12.0) / OSC_OVERSAMPLING, 0.707);
    }

    if (!oscdata->p[win_highcut].deactivated)
    {
        auto par = &(oscdata->p[win_highcut]);
        auto pv = limit_range(localcopy[par->param_id_in_scene].f, par->val_min.f, par->val_max.f);
        lp.coeff_LP2B(lp.calc_omega(pv / 12.0) / OSC_OVERSAMPLING, 0.707);
    }

    for (int k = 0; k < BLOCK_SIZE_OS; k += BLOCK_SIZE)
    {
        if (!oscdata->p[win_lowcut].deactivated)
            hp.process_block(&(output[k]), &(outputR[k]));
        if (!oscdata->p[win_highcut].deactivated)
            lp.process_block(&(output[k]), &(outputR[k]));
    }
}

void WindowOscillator::handleStreamingMismatches(int streamingRevision,
                                                 int currentSynthStreamingRevision)
{
    if (streamingRevision <= 12)
    {
        oscdata->p[win_lowcut].val.f = oscdata->p[win_lowcut].val_min.f; // high cut at the bottom
        oscdata->p[win_lowcut].deactivated = true;
        oscdata->p[win_highcut].val.f = oscdata->p[win_highcut].val_max.f; // low cut at the top
        oscdata->p[win_highcut].deactivated = true;
    }

    if (streamingRevision <= 15)
    {
        if (oscdata->p[win_unison_voices].val.i == 1)
        {
            oscdata->retrigger.val.b = true;
        }
    }
}
