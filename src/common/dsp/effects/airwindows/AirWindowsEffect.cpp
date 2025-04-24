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
#include "AirWindowsEffect.h"
#include "UserDefaults.h"
#include "DebugHelpers.h"

#include "sst/basic-blocks/mechanics/block-ops.h"
namespace mech = sst::basic_blocks::mechanics;

constexpr int subblock_factor = 3; // divide block by 2^this

std::vector<AirWinBaseClass::Registration> AirWindowsEffect::fxreg;
std::vector<int> AirWindowsEffect::fxregOrdering;
AirWindowsEffect::AWFxSelectorMapper AirWindowsEffect::mapper;

AirWindowsEffect::AirWindowsEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd)
    : Effect(storage, fxdata, pd)
{
    if (fxreg.empty())
    {
        fxreg = AirWinBaseClass::pluginRegistry();
        fxregOrdering = AirWinBaseClass::pluginRegistryOrdering();
    }

    for (int i = 0; i < n_fx_params - 1; i++)
    {
        param_lags[i].newValue(0);
        param_lags[i].instantize();
        param_lags[i].setRate(0.004 * (BLOCK_SIZE >> subblock_factor));
    }
}

AirWindowsEffect::~AirWindowsEffect() {}

void AirWindowsEffect::init()
{

    // std::cout << "AirWindows init " << std::endl;
    // for( int i=1;i<n_fx_params;++i)
    //   if( fxdata->p[i].ctrltype != ct_none )
    //      std::cout << _D(i) << _D(fxdata->p[i].val.f) << std::endl;
}

const char *AirWindowsEffect::group_label(int id)
{
    switch (id)
    {
    case 0:
        return "Type";
    case 1:
    {
        if (airwin)
        {
            static char txt[1024];
            strncpy(txt, mapper.nameAtStreamedIndex(fxdata->p[0].val.i).c_str(), 1023);
            return (const char *)txt;
        }
        else
        {
            return "Effect";
        }
    }
    }
    return 0;
}
int AirWindowsEffect::group_label_ypos(int id)
{
    switch (id)
    {
    case 0:
        return 1;
    case 1:
        return 5;
    }
    return 0;
}

void AirWindowsEffect::init_ctrltypes()
{
    /*
    ** This looks odd right? Why not just call resetCtrlTypes?
    ** Well: When we load if we are set to ct_none then we don't
    ** stream values on. So what we do is we transiently make
    ** every 1..n_fx a ct_airwindows_param so the unstream
    ** can set values, then when we process later, resetCtrlTypes
    ** will take those prior values and assign them as new (and that's
    ** what is called if the value is changed). Also since the load
    ** will often load to a separate instance and copy the params over
    ** we set the user_data to nullptr here to indicate that
    ** after this inti we need to do something even if the value
    ** of our FX hasn't changed.
    */
    Effect::init_ctrltypes();

    fxdata->p[0].set_name("FX");
    fxdata->p[0].set_type(ct_airwindows_fx);
    fxdata->p[0].posy_offset = 1;
    fxdata->p[0].val_max.i = fxreg.size() - 1;
    fxdata->p[0].set_user_data(nullptr);
    fxdata->p[0].deactivated = false;

    for (int i = 0; i < n_fx_params - 1; ++i)
    {
        fxdata->p[i + 1].set_type(ct_percent); // setting to ct_none means we don't stream onto this
        std::string w = "Param " + std::to_string(i);
        fxdata->p[i + 1].set_name(w.c_str());

        if (!fxFormatters[i])
            fxFormatters[i] = std::make_unique<AWFxParamFormatter>(this, i);
    }

    lastSelected = -1;
}

void AirWindowsEffect::resetCtrlTypes(bool useStreamedValues)
{
    fxdata->p[0].set_name("FX");
    fxdata->p[0].set_type(ct_airwindows_fx);
    fxdata->p[0].posy_offset = 1;
    fxdata->p[0].val_max.i = fxreg.size() - 1;

    fxdata->p[0].set_user_data(&mapper);
    if (airwin)
    {
        for (int i = 0; i < airwin->paramCount && i < n_fx_params - 1; ++i)
        {
            char txt[1024];
            airwin->getParameterName(i, txt);
            auto priorVal = fxdata->p[i + 1].val.f;
            fxdata->p[i + 1].set_name(txt);
            if (airwin->isParameterIntegral(i))
            {
                fxdata->p[i + 1].set_type(ct_airwindows_param_integral);
                fxdata->p[i + 1].val_min.i = 0;
                fxdata->p[i + 1].val_max.i = airwin->parameterIntegralUpperBound(i);

                if (useStreamedValues)
                    fxdata->p[i + 1].val.i =
                        (int)(priorVal * (airwin->parameterIntegralUpperBound(i) + 0.999));
                else
                    fxdata->p[i + 1].val.i =
                        (int)(airwin->getParameter(i) *
                              (airwin->parameterIntegralUpperBound(i) + 0.999));
            }
            else if (airwin->isParameterBipolar(i))
            {
                fxdata->p[i + 1].set_type(ct_airwindows_param_bipolar);
            }
            else
            {
                fxdata->p[i + 1].set_type(ct_airwindows_param);
            }
            fxdata->p[i + 1].set_user_data(fxFormatters[i].get());
            fxdata->p[i + 1].posy_offset = 3;

            if (useStreamedValues)
            {
                fxdata->p[i + 1].val.f = priorVal;
            }
            else
                fxdata->p[i + 1].val.f = airwin->getParameter(i);
        }

        // set any FX parameters current Airwindows effect isn't using to none/generic param name
        for (int i = airwin->paramCount; i < n_fx_params - 1;
             ++i) // -1 since we have +1 in the indexing since 0 is type
        {
            fxdata->p[i + 1].set_type(ct_none);
            std::string w = "Param " + std::to_string(i);
            fxdata->p[i + 1].set_name(w.c_str());
        }
    }

    hasInvalidated = true;
}

void AirWindowsEffect::init_default_values()
{
    fxdata->p[0].val.i = 0;
    for (int i = 0; i < 10; ++i)
        fxdata->p[i + 1].val.f = 0;
}

void AirWindowsEffect::process(float *dataL, float *dataR)
{
    if (fxdata->p[0].deactivated)
    {
        // We are un-suspended
        fxdata->p[0].deactivated = false;
        hasInvalidated = true;
    }

    if (!airwin || fxdata->p[0].val.i != lastSelected || fxdata->p[0].user_data == nullptr)
    {
        /*
        ** So do we want to let Airwindows set params as defaults or do we want
        ** to use the values on our params if we recreate? Well we have two cases.
        ** If the userdata on p0 is null it means we have unstreamed something but
        ** we have not set up Airwindows. So this means we are loading an FXP,
        ** a config XML snapshot, or similar.
        **
        ** If the userdata is set up and the last selected is changed then that
        ** means we have used a UI or automation gesture to re-modify a current
        ** running Airwindows effect, so apply the defaults
        */
        bool useStreamedValues = false;
        if (fxdata->p[0].user_data == nullptr)
        {
            useStreamedValues = true;
        }
        setupSubFX(fxdata->p[0].val.i, useStreamedValues);
    }

    if (!airwin)
        return;

    // See #4900
    if (airwin->denormBeforeProcess)
    {
        for (int i = 0; i < BLOCK_SIZE; ++i)
        {
            if (fabs(dataL[i]) <= 2e-15)
                dataL[i] = 0;
            if (fabs(dataR[i]) <= 2e-15)
                dataR[i] = 0;
        }
    }

    constexpr int QBLOCK = BLOCK_SIZE >> subblock_factor;
    float outL alignas(16)[BLOCK_SIZE], outR alignas(16)[BLOCK_SIZE];

    for (int subb = 0; subb < 1 << subblock_factor; ++subb)
    {
        for (int i = 0; i < airwin->paramCount && i < n_fx_params - 1; ++i)
        {
            param_lags[i].newValue(clamp01(*pd_float[i + 1]));
            if (fxdata->p[i + 1].ctrltype == ct_airwindows_param_integral)
            {
                airwin->setParameter(i, fxdata->p[i + 1].get_value_f01());
            }
            else
            {
                airwin->setParameter(i, param_lags[i].v);
            }
            param_lags[i].process();
        }

        float *in[2];
        in[0] = dataL + subb * QBLOCK;
        in[1] = dataR + subb * QBLOCK;

        float *out[2];
        out[0] = &(outL[0]) + subb * QBLOCK;
        out[1] = &(outR[0]) + subb * QBLOCK;

        airwin->processReplacing(in, out, QBLOCK);
    }

    mech::copy_from_to<BLOCK_SIZE>(outL, dataL);
    mech::copy_from_to<BLOCK_SIZE>(outR, dataR);
}

void AirWindowsEffect::setupSubFX(int sfx, bool useStreamedValues)
{
    const auto &r = fxreg[sfx];
    const bool detailedMode = Surge::Storage::getValueDisplayIsHighPrecision(storage);
    int dp = detailedMode ? 6 : 2;

    airwin = r.create(r.id, storage->dsamplerate, dp); // FIXME
    airwin->storage = storage;

    char fxname[1024];
    airwin->getEffectName(fxname);
    lastSelected = sfx;
    resetCtrlTypes(useStreamedValues);

    // Snap the init values as defaults onto the params. Start at 1 since slot 0 is FX type selector
    for (auto i = 1; i < n_fx_params; ++i)
    {
        if (fxdata->p[i].ctrltype != ct_none)
        {
            fxdata->p[i].val_default.f = fxdata->p[i].val.f;
        }
    }
}

void AirWindowsEffect::updateAfterReload()
{
    fxdata->p[0].deactivated = true; // assume I'm suspended unless I run
    setupSubFX(fxdata->p[0].val.i, true);
}
