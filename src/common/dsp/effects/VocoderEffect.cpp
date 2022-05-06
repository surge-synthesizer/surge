#include "VocoderEffect.h"
#include <algorithm>

// Hz from http://www.sequencer.de/pix/moog/moog_vocoder_rack.jpg
// const float vocoder_freq_moog[n_vocoder_bands] = {50, 158, 200, 252, 317, 400, 504, 635, 800,
// 1008, 1270, 1600, 2016, 2504, 3200, 4032, 5080}; const float vocoder_freq[n_vocoder_bands] =
// {100, 175, 230, 290, 360, 450, 580, 700, 900, 1100, 1400, 1800, 2300, 2800, 3600, 4700};

// const float vocoder_freq[n_vocoder_bands] = {100, 175, 230, 290, 360, 450, 580, 700, 900, 1100,
// 1400, 1800, 2300, 3000, 4500, 8000}; const float vocoder_freq[n_vocoder_bands] = {158, 200, 252,
// 317, 400, 504, 635, 800, 1008, 1270, 1600, 2016, 2504, 3200, 4032, 5080};

// const float vocoder_freq_vsm201[n_vocoder_bands] = {190, 290, 400, 510, 630, 760, 910, 1080,
// 1270, 1480, 1700, 2000, 2300, 2700, 3100, 3700, 4400, 5200, 6600, 8000}; const float
// vocoder_freq_vsm201[n_vocoder_bands] = {170, 240, 340, 440, 560, 680, 820, 970, 1150, 1370, 1605,
// 1850, 2150, 2500, 2900, 3400, 4050, 4850, 5850, 7500};

const float vocoder_freq_vsm201[n_vocoder_bands] = {180,  219,  266,  324,  394,  480,  584,
                                                    711,  865,  1053, 1281, 1559, 1898, 2309,
                                                    2810, 3420, 4162, 5064, 6163, 7500};

//------------------------------------------------------------------------------------------------

VocoderEffect::VocoderEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd)
    : Effect(storage, fxdata, pd), mBI(0)
{
    /*mVoicedDetect.storage = storage;
    mUnvoicedDetect.storage = storage;
    mVoicedLevel = 0.f;
    mUnvoicedLevel = 0.f;*/

    active_bands = n_vocoder_bands;
    mGain.set_blocksize(BLOCK_SIZE);
    mGainR.set_blocksize(BLOCK_SIZE);
    for (int i = 0; i < voc_vector_size; i++)
    {
        mEnvF[i] = vZero;
        mEnvFR[i] = vZero;
    }
}

//------------------------------------------------------------------------------------------------

VocoderEffect::~VocoderEffect() {}

//------------------------------------------------------------------------------------------------

void VocoderEffect::init() { setvars(true); }

//------------------------------------------------------------------------------------------------

void VocoderEffect::setvars(bool init)
{
    modulator_mode = *f[voc_mod_input];
    wet = *f[voc_mix];
    float Freq[4], FreqM[4];

    const float Q = 20.f * (1.f + 0.5f * *f[voc_q]);
    const float Spread = 0.4f / Q;

    active_bands = *pdata_ival[voc_num_bands];
    // FIXME - adjust the UI to be chunks of 4
    active_bands = active_bands - (active_bands % 4);

    // We need to clamp these in reasonable ranges
    float flo = limit_range(*f[voc_minfreq], -36.f, 36.f);
    float fhi = limit_range(*f[voc_maxfreq], 0.f, 60.f);

    if (flo > fhi)
    {
        auto t = fhi;
        fhi = flo;
        flo = t;
    }
    float df = (fhi - flo) / (active_bands - 1);

    float hzlo = 440.f * pow(2.f, flo / 12.f);
    float dhz = pow(2.f, df / 12.f);

    float fb = hzlo;

    float mb = fb;
    float mdhz = dhz;
    bool sepMod = false;

    float mC = *f[voc_mod_center];
    float mX = *f[voc_mod_range];

    if (mC != 0 || mX != 0)
    {
        sepMod = true;
        auto fDist = fhi - flo;
        auto fDistHalf = fDist / 2.f;

        // that 0.3 is a tuning choice about how far we can move center
        auto mMid = fDistHalf + flo + 0.3 * mC * fDistHalf;
        // as is that 0.7
        auto mLo = mMid - fDistHalf * (1 + 0.7 * mX);
        auto dM = fDistHalf * 2 * (1.0 + 0.7 * mX) / (active_bands - 1);

        auto mHi = mLo + dM * (active_bands - 1);

        if (mHi > 60)
            dM = (60 - mLo) / (active_bands - 1);

        mHi = mLo + dM * (active_bands - 1);

        mb = 440.0 * pow(2.f, mLo / 12.f);
        mdhz = pow(2.f, dM / 12.f);
    }

    for (int i = 0; i < active_bands && i < n_vocoder_bands; i++)
    {
        Freq[i & 3] = fb * storage->samplerate_inv;
        FreqM[i & 3] = mb * storage->samplerate_inv;

        if ((i & 3) == 3)
        {
            int j = i >> 2;
            mCarrierL[j].SetCoeff(Freq, Q, Spread);
            mCarrierR[j].CopyCoeff(mCarrierL[j]);
            if (sepMod)
            {
                mModulator[j].SetCoeff(FreqM, Q, Spread);
                if (modulator_mode == vim_stereo)
                {
                    mModulatorR[j].SetCoeff(FreqM, Q, Spread);
                }
                else
                {
                    mModulatorR[j].CopyCoeff(mModulator[j]);
                }
            }
            else
            {
                mModulator[j].CopyCoeff(mCarrierL[j]);
                mModulatorR[j].CopyCoeff(mCarrierR[j]);
            }
        }
        fb *= dhz;
        mb *= mdhz;
    }

    /*mVoicedDetect.coeff_LP(BiquadFilter::calc_omega_from_Hz(1000.f), 0.707);
    mUnvoicedDetect.coeff_HP(BiquadFilter::calc_omega_from_Hz(5000.f), 0.707);

    if (init)
    {
       mVoicedDetect.coeff_instantize();
       mUnvoicedDetect.coeff_instantize();
    }*/
}

//------------------------------------------------------------------------------------------------

void VocoderEffect::process(float *dataL, float *dataR)
{
    mBI = (mBI + 1) & 0x3f;

    if (mBI == 0)
    {
        setvars(false);
    }
    modulator_mode = fxdata->p[voc_mod_input].val.i;
    wet = *f[voc_mix];
    float EnvFRate = 0.001f * powf(2.f, 4.f * *f[voc_envfollow]);

    // the left channel variables are used for mono when stereo is disabled
    float modulator_in alignas(16)[BLOCK_SIZE];
    float modulator_inR alignas(16)[BLOCK_SIZE];

    if (modulator_mode == vim_mono)
    {
        add_block(storage->audio_in_nonOS[0], storage->audio_in_nonOS[1], modulator_in,
                  BLOCK_SIZE_QUAD);
    }
    else
    {
        copy_block(storage->audio_in_nonOS[0], modulator_in, BLOCK_SIZE_QUAD);
        copy_block(storage->audio_in_nonOS[1], modulator_inR, BLOCK_SIZE_QUAD);
    }

    float Gain = *f[voc_input_gain] + 24.f;
    mGain.set_target_smoothed(storage->db_to_linear(Gain));
    mGain.multiply_block(modulator_in, BLOCK_SIZE_QUAD);

    mGainR.set_target_smoothed(storage->db_to_linear(Gain));
    mGainR.multiply_block(modulator_inR, BLOCK_SIZE_QUAD);

    vFloat Rate = vLoad1(EnvFRate);
    vFloat Ratem1 = vLoad1(1.f - EnvFRate);

    float Gate = storage->db_to_linear(*f[voc_input_gate] + Gain);
    vFloat GateLevel = vLoad1(Gate * Gate);

    const vFloat MaxLevel = vLoad1(6.f);

    // Voiced / Unvoiced detection
    /*   mVoicedDetect.process_block_to(modulator_in, modulator_tbuf);
       float a = min(4.f, get_squaremax(modulator_tbuf,BLOCK_SIZE_QUAD));
       mVoicedLevel = mVoicedLevel * (1.f - EnvFRate) + a * EnvFRate;

       mUnvoicedDetect.process_block_to(modulator_in, modulator_tbuf);
       a = min(4.f, get_squaremax(modulator_tbuf, BLOCK_SIZE_QUAD));
       mUnvoicedLevel = mUnvoicedLevel * (1.f - EnvFRate) + a * EnvFRate;

       float Ratio = db_to_linear(*f[voc_unvoiced_threshold]);

       if (mUnvoicedLevel > (mVoicedLevel * Ratio))
         // mix carrier with noise
         for(int i = 0; i < BLOCK_SIZE; i++)
         {
            float rand11 = storage->rand_pm1();
            dataL[i] = rand11;
            rand11 = storage->rand_pm1();
            dataR[i] = rand11;
         }*/

    if (modulator_mode == vim_mono || modulator_mode == vim_left || modulator_mode == vim_right)
    {
        float *input;

        if (modulator_mode == vim_mono || modulator_mode == vim_left)
        {
            input = modulator_in;
        }
        else
        {
            input = modulator_inR;
        }

        for (int k = 0; k < BLOCK_SIZE; k++)
        {
            vFloat In = vLoad1(input[k]);

            vFloat Left = vLoad1(dataL[k]);
            vFloat Right = vLoad1(dataR[k]);

            vFloat LeftSum = vZero;
            vFloat RightSum = vZero;

            for (int j = 0; (j < (active_bands >> 2)) && (j < voc_vector_size); j++)
            {
                vFloat Mod = mModulator[j].CalcBPF(In);
                Mod = vMin(vMul(Mod, Mod), MaxLevel);
                Mod = vAnd(Mod, vCmpGE(Mod, GateLevel));
                mEnvF[j] = vMAdd(mEnvF[j], Ratem1, vMul(Rate, Mod));
                Mod = vSqrtFast(mEnvF[j]);

                LeftSum = vAdd(LeftSum, mCarrierL[j].CalcBPF(vMul(Left, Mod)));
                RightSum = vAdd(RightSum, mCarrierR[j].CalcBPF(vMul(Right, Mod)));
            }

            float inMul = 1.0 - wet;
            dataL[k] = dataL[k] * inMul + wet * vSum(LeftSum) * 4.f;
            dataR[k] = dataR[k] * inMul + wet * vSum(RightSum) * 4.f;
        }
    }
    else if (modulator_mode == vim_stereo)
    {
        for (int k = 0; k < BLOCK_SIZE; k++)
        {
            vFloat InL = vLoad1(modulator_in[k]);
            vFloat InR = vLoad1(modulator_inR[k]);
            vFloat Left = vLoad1(dataL[k]);
            vFloat Right = vLoad1(dataR[k]);

            vFloat LeftSum = vZero;
            vFloat RightSum = vZero;

            for (int j = 0; j < (active_bands >> 2) && j < (n_vocoder_bands >> 2); j++)
            {
                vFloat ModL = mModulator[j].CalcBPF(InL);
                vFloat ModR = mModulatorR[j].CalcBPF(InR);
                ModL = vMin(vMul(ModL, ModL), MaxLevel);
                ModR = vMin(vMul(ModR, ModR), MaxLevel);

                ModL = vAnd(ModL, vCmpGE(ModL, GateLevel));
                ModR = vAnd(ModR, vCmpGE(ModR, GateLevel));

                mEnvF[j] = vMAdd(mEnvF[j], Ratem1, vMul(Rate, ModL));
                mEnvFR[j] = vMAdd(mEnvFR[j], Ratem1, vMul(Rate, ModR));
                ModL = vSqrtFast(mEnvF[j]);
                ModR = vSqrtFast(mEnvFR[j]);
                LeftSum = vAdd(LeftSum, mCarrierL[j].CalcBPF(vMul(Left, ModL)));
                RightSum = vAdd(RightSum, mCarrierR[j].CalcBPF(vMul(Right, ModR)));
            }

            float inMul = 1.0 - wet;
            dataL[k] = dataL[k] * inMul + wet * vSum(LeftSum) * 4.f;
            dataR[k] = dataR[k] * inMul + wet * vSum(RightSum) * 4.f;
        }
    }
}

//------------------------------------------------------------------------------------------------

void VocoderEffect::suspend() { init(); }

//------------------------------------------------------------------------------------------------

void VocoderEffect::init_default_values()
{
    fxdata->p[voc_input_gain].val.f = 0.f;
    fxdata->p[voc_input_gate].val.f = -96.f;
    fxdata->p[voc_envfollow].val.f = 0.f;
    fxdata->p[voc_q].val.f = 0.f;

    fxdata->p[voc_num_bands].val.i = n_vocoder_bands;

    fxdata->p[voc_minfreq].val.f = 12.f * log(vocoder_freq_vsm201[0] / 440.f) / log(2.f);
    fxdata->p[voc_maxfreq].val.f =
        12.f * log(vocoder_freq_vsm201[n_vocoder_bands - 1] / 440.f) / log(2.f);

    fxdata->p[voc_mod_range].val.f = 0.f;
    fxdata->p[voc_mod_center].val.f = 0.f;

    fxdata->p[voc_mod_input].val.i = 0;
    fxdata->p[voc_mix].val.f = 1.f;
}

//------------------------------------------------------------------------------------------------

const char *VocoderEffect::group_label(int id)
{
    switch (id)
    {
    case 0:
        return "Input";
    case 1:
        return "Filter Bank";
    case 2:
        return "Carrier";
    case 3:
        return "Modulator";
    case 4:
        return "Output";
    }
    return 0;
}

//------------------------------------------------------------------------------------------------

int VocoderEffect::group_label_ypos(int id)
{
    switch (id)
    {
    case 0:
        return 1;
    case 1:
        return 7;
    case 2:
        return 13;
    case 3:
        return 21;
    case 4:
        return 29;
    }
    return 0;
}

//------------------------------------------------------------------------------------------------

void VocoderEffect::init_ctrltypes()
{
    Effect::init_ctrltypes();

    fxdata->p[voc_input_gain].set_name("Gain");
    fxdata->p[voc_input_gain].set_type(ct_decibel);
    fxdata->p[voc_input_gain].posy_offset = 1;

    fxdata->p[voc_input_gate].set_name("Gate");
    fxdata->p[voc_input_gate].set_type(ct_decibel_attenuation_large);
    fxdata->p[voc_input_gate].posy_offset = 1;

    fxdata->p[voc_envfollow].set_name("Env Follow");
    fxdata->p[voc_envfollow].set_type(ct_percent);
    fxdata->p[voc_envfollow].posy_offset = 3;

    /*fxdata->p[voc_unvoiced_threshold].set_name("Unvoiced Thresh");
    fxdata->p[voc_unvoiced_threshold].set_type(ct_decibel);
    fxdata->p[voc_unvoiced_threshold].posy_offset = 3;*/

    fxdata->p[voc_q].set_name("Q");
    fxdata->p[voc_q].set_type(ct_percent_bipolar);
    fxdata->p[voc_q].posy_offset = 3;

    fxdata->p[voc_num_bands].set_name("Bands");
    fxdata->p[voc_num_bands].set_type(ct_vocoder_bandcount);
    fxdata->p[voc_num_bands].posy_offset = 3;

    fxdata->p[voc_minfreq].set_name("Min Frequency");
    fxdata->p[voc_minfreq].set_type(ct_freq_vocoder_low);
    fxdata->p[voc_minfreq].posy_offset = 3;

    fxdata->p[voc_maxfreq].set_name("Max Frequency");
    fxdata->p[voc_maxfreq].set_type(ct_freq_vocoder_high);
    fxdata->p[voc_maxfreq].posy_offset = 3;

    fxdata->p[voc_mod_input].set_name("Input");
    fxdata->p[voc_mod_input].set_type(ct_vocoder_modulator_mode);
    fxdata->p[voc_mod_input].posy_offset = 5;

    fxdata->p[voc_mod_range].set_name("Range");
    fxdata->p[voc_mod_range].set_type(ct_percent_bipolar);
    fxdata->p[voc_mod_range].posy_offset = 5;

    fxdata->p[voc_mod_center].set_name("Center");
    fxdata->p[voc_mod_center].set_type(ct_percent_bipolar);
    fxdata->p[voc_mod_center].posy_offset = 5;

    fxdata->p[voc_mix].set_name("Mix");
    fxdata->p[voc_mix].set_type(ct_percent);
    fxdata->p[voc_mix].posy_offset = 7;
}

void VocoderEffect::handleStreamingMismatches(int streamingRevision,
                                              int currentSynthStreamingRevision)
{
    if (streamingRevision <= 10)
    {
        fxdata->p[voc_num_bands].val.i = n_vocoder_bands;

        fxdata->p[voc_minfreq].val.f = 12.f * log(vocoder_freq_vsm201[0] / 440.f) / log(2.f);
        fxdata->p[voc_maxfreq].val.f =
            12.f * log(vocoder_freq_vsm201[n_vocoder_bands - 1] / 440.f) / log(2.f);

        fxdata->p[voc_mod_range].val.f = 0.f;
        fxdata->p[voc_mod_center].val.f = 0.f;

        fxdata->p[voc_mod_input].val.i = 0;
        fxdata->p[voc_mix].val.f = 1.f;
    }
}
//------------------------------------------------------------------------------------------------
