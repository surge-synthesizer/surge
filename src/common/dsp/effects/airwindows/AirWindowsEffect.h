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

#ifndef SURGE_SRC_COMMON_DSP_EFFECTS_AIRWINDOWS_AIRWINDOWSEFFECT_H
#define SURGE_SRC_COMMON_DSP_EFFECTS_AIRWINDOWS_AIRWINDOWSEFFECT_H

#include "Effect.h"
#include "dsp/effects/ControlPreparationMailbox.h"
#include "airwindows/AirWinBaseClass.h"

#include <vector>
#include "UserDefaults.h"
#include "StringOps.h"

class alignas(16) AirWindowsEffect : public Effect
{
  public:
    AirWindowsEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd);
    virtual ~AirWindowsEffect();

    virtual const char *get_effectname() override { return "Airwindows"; };

    virtual void init() override;
    virtual void init_ctrltypes() override;
    virtual void init_default_values() override;

    virtual void updateAfterReload() override;

    void resetCtrlTypes(bool useStreamedValues);

    virtual void process(float *dataL, float *dataR) override;

    virtual const char *group_label(int id) override;
    virtual int group_label_ypos(int id) override;

    // TODO ringout and only control and suspend
    virtual void suspend() override
    {
        hasInvalidated = true;

        if (fxdata)
        {
            fxdata->p[0].deactivated = true;
            // The retired DeEss/Tube slots are already prepared, stateless
            // silence processors with no parameters. ct_none is their valid
            // metadata, not evidence that construction must be repeated.
            const bool preparedRetired = dynamic_cast<AirWindowsNoOp *>(airwin.get()) &&
                                         lastSelected == fxdata->p[0].val.i &&
                                         fxdata->p[0].user_data != nullptr;
            if (fxdata->p[1].ctrltype == ct_none && !preparedRetired)
            {
                setupSubFX(fxdata->p[0].val.i, true);
            }
        }
    }

    lag<float, true> param_lags[n_fx_params - 1];

    // Prepare on the control thread, then adopt under exclusive engine ownership.
    // Keep the consumed preparation alive for control-thread reclamation: it
    // owns the outgoing processor and metadata after the exchange.
    struct SelectionParameterState
    {
        pdata value{};
        bool temposync{}, absolute{}, deactivated{}, extend_range{};
        bool porta_constrate{}, porta_gliss{}, porta_retrigger{};
        int porta_curve{}, deform_type{}, midictrl{}, midichan{};
        SoftTakeoverStatus takeover{};
        SelectionParameterState() = default;
        explicit SelectionParameterState(const Parameter &p)
            : value(p.val), temposync(p.temposync), absolute(p.absolute),
              deactivated(p.deactivated), extend_range(p.extend_range),
              porta_constrate(p.porta_constrate), porta_gliss(p.porta_gliss),
              porta_retrigger(p.porta_retrigger), porta_curve(p.porta_curve),
              deform_type(p.deform_type), midictrl(p.midictrl), midichan(p.midichan),
              takeover(p.miditakeover_status) {}
        void apply(Parameter &p) const
        {
            p.val = value;
#define APPLY_SELECTION_FIELD(field) p.field = field;
            APPLY_SELECTION_FIELD(temposync)
            APPLY_SELECTION_FIELD(absolute)
            APPLY_SELECTION_FIELD(deactivated)
            APPLY_SELECTION_FIELD(extend_range)
            APPLY_SELECTION_FIELD(porta_constrate)
            APPLY_SELECTION_FIELD(porta_gliss)
            APPLY_SELECTION_FIELD(porta_retrigger)
            APPLY_SELECTION_FIELD(porta_curve)
            APPLY_SELECTION_FIELD(deform_type)
            APPLY_SELECTION_FIELD(midictrl)
            APPLY_SELECTION_FIELD(midichan)
#undef APPLY_SELECTION_FIELD
            p.miditakeover_status = takeover;
        }
    };
    struct SelectionRequest
    {
        std::array<SelectionParameterState, n_fx_params> parameters{};
        std::array<int, n_fx_params> parameterIds{};
        const AirWindowsEffect *owner{nullptr};
        float sampleRate{};
        int selector{-1}, previousSelector{-1};
    };
    // Capture only fixed-size editable state on the serialized audio owner.
    // The control caller supplies an immutable metadata basis with this slot's
    // parameter identities; preparing it must not read the live parameter array.
    SelectionRequest captureSelectionRequest(int selector) const noexcept;
    struct PreparedSelection;
    std::unique_ptr<PreparedSelection> prepareSelection(
        const FxStorage &basis, const SelectionRequest &request) const;
    struct PreparedSelection
    {
        explicit PreparedSelection(const FxStorage &source) : parameters(source) {}
        FxStorage parameters;
        std::unique_ptr<AirWinBaseClass> processor;
        const AirWindowsEffect *owner{nullptr};
        int selector{-1}, previousSelector{-1};
        float sampleRate{};
        bool consumed{false};
    };
    std::unique_ptr<PreparedSelection> prepareSelection(int selector) const;
    bool adoptSelection(PreparedSelection &selection) noexcept;

    void serviceBrowserSelection(const FxStorage &basis);
    bool processBrowserSelection(int wanted, bool &finished) noexcept;
    ControlPreparationMailbox<SelectionRequest, PreparedSelection> selectionMailbox;

    void setupSubFX(int awfx, bool useStreamedValues);
    std::unique_ptr<AirWinBaseClass> airwin;
    int lastSelected = -1;

    void sampleRateReset() override
    {
        if (airwin)
        {
            airwin->sr = storage->samplerate;
        }
    }

    static std::vector<AirWinBaseClass::Registration> fxreg;
    static std::vector<int> fxregOrdering;

    static struct AWFxSelectorMapper : public ParameterDiscreteIndexRemapper
    {
        AWFxSelectorMapper() {}

        // See #6619 for this defense which works around a different problem
        inline int safeIdx(int i) const { return std::clamp(i, 0, (int)fxreg.size() - 1); }
        virtual int remapStreamedIndexToDisplayIndex(int i) const override
        {
            return fxreg[safeIdx(i)].displayOrder;
        }
        virtual std::string nameAtStreamedIndex(int i) const override
        {
            return fxreg[safeIdx(i)].name;
        }
        virtual bool hasGroupNames() const override { return true; }

        virtual std::string groupNameAtStreamedIndex(int i) const override
        {
            return fxreg[safeIdx(i)].groupName;
        }

        bool supportsTotalIndexOrdering() const override { return true; }

        const std::vector<int> totalIndexOrdering() const override { return fxregOrdering; }
        AirWindowsEffect *fx;
    } mapper;

    struct AWFxParamFormatter : public ParameterExternalFormatter
    {
        AWFxParamFormatter(AirWindowsEffect *fx, int i) : fx(fx), idx(i) {}
        virtual bool formatValue(const Parameter *p, float value, char *txt,
                                 int txtlen) const override
        {
            if (fx && fx->airwin)
            {
                char lab[TXT_SIZE], dis[TXT_SIZE];
                // In case we aren't initialized by the AW
                lab[0] = 0;
                dis[0] = 0;
                if (fx->airwin->isParameterIntegral(idx))
                {
                    fx->airwin->getIntegralDisplayForValue(idx, value, dis);
                    lab[0] = 0;
                }
                else
                {
                    if (fx->fxdata->p[0].deactivated)
                    {
                        fx->airwin->setParameter(idx, value);
                    }

                    fx->airwin->displayPrecision =
                        Surge::Storage::getValueDisplayPrecision(fx->storage);
                    fx->airwin->getParameterLabel(idx, lab);
                    fx->airwin->getParameterDisplay(idx, dis, value, true);
                }
                snprintf(txt, TXT_SIZE, "%s%s%s", dis, (lab[0] == 0 ? "" : " "), lab);
            }
            else
            {
                snprintf(txt, TXT_SIZE, "AWA.ERROR %lf", value);
            }
            return true;
        }

        virtual bool stringToValue(const Parameter *p, const char *txt,
                                   float &outVal) const override
        {
            if (fx && fx->airwin)
            {
                float v;

                if (fx->airwin->parseParameterValueFromString(idx, txt, v))
                {
                    if (v < 0.f || v > 1.f)
                    {
                        return false;
                    }

                    outVal = v;
                    return true;
                }
            }
            return false;
        }
        AirWindowsEffect *fx;
        int idx;
    };

    std::array<std::unique_ptr<AWFxParamFormatter>, n_fx_params - 1> fxFormatters;
};

#endif // SURGE_SRC_COMMON_DSP_EFFECTS_AIRWINDOWS_AIRWINDOWSEFFECT_H
