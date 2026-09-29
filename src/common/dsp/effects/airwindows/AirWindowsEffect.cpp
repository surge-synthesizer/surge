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
#include <stdexcept>
#include <type_traits>
#if SURGE_WEB
bool surge_convolution_is_realtime();
namespace { std::atomic<unsigned> realtimeAirwindowsConstructions{0}; }
unsigned surge_airwindows_realtime_constructions()
{
    return realtimeAirwindowsConstructions.load(std::memory_order_relaxed);
}
#endif
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

AirWindowsEffect::SelectionRequest
AirWindowsEffect::captureSelectionRequest(int selector) const noexcept
{
    static_assert(std::is_trivially_copyable_v<SelectionRequest>);
    SelectionRequest request;
    request.owner = this;
    request.selector = selector;
    request.previousSelector = lastSelected;
    request.sampleRate = storage->samplerate;
    for (int i = 0; i < n_fx_params; ++i)
    {
        request.parameters[i] = SelectionParameterState(fxdata->p[i]);
        request.parameterIds[i] = fxdata->p[i].id;
    }
    return request;
}

std::unique_ptr<AirWindowsEffect::PreparedSelection>
AirWindowsEffect::prepareSelection(int selector) const
{
    // Exclusive-owner convenience path used by the native reference harness.
    return prepareSelection(*fxdata, captureSelectionRequest(selector));
}

std::unique_ptr<AirWindowsEffect::PreparedSelection>
AirWindowsEffect::prepareSelection(const FxStorage &basis, const SelectionRequest &request) const
{
    if (request.owner != this || request.selector < 0 || request.selector >= fxreg.size() ||
        request.sampleRate != storage->samplerate)
        throw std::out_of_range("Invalid or obsolete Airwindows selection request");
    for (const auto &formatter : fxFormatters)
        if (!formatter)
            throw std::logic_error("Airwindows metadata must be initialized before selection");
    for (int i = 0; i < n_fx_params; ++i)
        if (basis.p[i].id != request.parameterIds[i])
            throw std::invalid_argument("Airwindows selection metadata belongs to another slot");
    auto next = std::make_unique<PreparedSelection>(basis);
    std::array<pdata, n_global_params> privateValues{};
    AirWindowsEffect builder(storage, &next->parameters, privateValues.data());
    builder.init_ctrltypes();
    // init_ctrltypes only initializes the builder's formatters. Reconstruct
    // editable state from the audio-owned snapshot, never from live parameters.
    next->parameters = basis;
    for (int i = 0; i < n_fx_params; ++i)
        request.parameters[i].apply(next->parameters.p[i]);
    next->parameters.p[0].val.i = request.selector;
    builder.setupSubFX(request.selector, false);
    next->processor = std::move(builder.airwin);
    next->owner = this;
    next->selector = request.selector;
    next->previousSelector = request.previousSelector;
    next->sampleRate = request.sampleRate;
    next->parameters.p[0].set_user_data(&mapper);
    for (int i = 0; i < next->processor->paramCount && i < n_fx_params - 1; ++i)
        next->parameters.p[i + 1].set_user_data(fxFormatters[i].get());
    return next;
}

bool AirWindowsEffect::adoptSelection(PreparedSelection &selection) noexcept
{
    static_assert(std::is_nothrow_move_constructible_v<Parameter>);
    static_assert(std::is_nothrow_move_assignable_v<Parameter>);
    if (selection.owner != this || selection.consumed || !selection.processor ||
        selection.previousSelector != lastSelected || selection.sampleRate != storage->samplerate)
        return false;
    for (int i = 0; i < n_fx_params; ++i)
    {
        // Direct selection never changes the optional oscillator alias. Keep
        // its live ownership rather than copying a string into the request.
        fxdata->p[i].oscName.swap(selection.parameters.p[i].oscName);
        std::swap(fxdata->p[i], selection.parameters.p[i]);
    }
    airwin.swap(selection.processor);
    lastSelected = selection.selector;
    fxdata->p[0].deactivated = false;
    hasInvalidated = true;
    selection.consumed = true;
    // param_lags and pd_float remain attached to this instance. The first
    // processing block sees the same pre-selection values as the native path.
    return true;
}

void AirWindowsEffect::serviceBrowserSelection(const FxStorage &basis)
{
    selectionMailbox.service([&](const SelectionRequest &request) {
        return prepareSelection(basis, request);
    });
}

bool AirWindowsEffect::processBrowserSelection(int wanted, bool &finished) noexcept
{
    bool adopted = false;
    finished = false;
    selectionMailbox.consume([&](const SelectionRequest &request, PreparedSelection *prepared) noexcept {
        if (wanted != request.selector) return; // Superseded request; control reclaims it.
        if (!prepared) { finished = true; return; }
        for (int i = 0; i < n_fx_params; ++i)
        {
            const auto &before = request.parameters[i];
            const auto &now = fxdata->p[i];
            if (before.value.i != now.val.i || before.takeover != now.miditakeover_status) return;
#define CHECK_SELECTION_FIELD(field) if (before.field != now.field) return;
            CHECK_SELECTION_FIELD(temposync)
            CHECK_SELECTION_FIELD(absolute)
            CHECK_SELECTION_FIELD(deactivated)
            CHECK_SELECTION_FIELD(extend_range)
            CHECK_SELECTION_FIELD(porta_constrate)
            CHECK_SELECTION_FIELD(porta_gliss)
            CHECK_SELECTION_FIELD(porta_retrigger)
            CHECK_SELECTION_FIELD(porta_curve)
            CHECK_SELECTION_FIELD(deform_type)
            CHECK_SELECTION_FIELD(midictrl)
            CHECK_SELECTION_FIELD(midichan)
#undef CHECK_SELECTION_FIELD
        }
        adopted = adoptSelection(*prepared);
        finished = adopted;
    });
    if (wanted >= 0 && wanted == lastSelected) finished = true;
    if (!finished && wanted >= 0 && wanted < fxreg.size())
        selectionMailbox.submit(captureSelectionRequest(wanted));
    return adopted;
}

void AirWindowsEffect::setupSubFX(int sfx, bool useStreamedValues)
{
#if SURGE_WEB
    if (surge_convolution_is_realtime())
        realtimeAirwindowsConstructions.fetch_add(1, std::memory_order_relaxed);
#endif
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
