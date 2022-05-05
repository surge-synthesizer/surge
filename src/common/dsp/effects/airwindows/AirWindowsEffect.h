// -*-c++-*-

#pragma once

#include "Effect.h"
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
            if (fxdata->p[1].ctrltype == ct_none)
            {
                setupSubFX(fxdata->p[0].val.i, true);
            }
        }
    }

    lag<float, true> param_lags[n_fx_params - 1];

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

        virtual int remapStreamedIndexToDisplayIndex(int i) const override
        {
            return fxreg[i].displayOrder;
        }
        virtual std::string nameAtStreamedIndex(int i) const override { return fxreg[i].name; }
        virtual bool hasGroupNames() const override { return true; }

        virtual std::string groupNameAtStreamedIndex(int i) const override
        {
            return fxreg[i].groupName;
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

                    if (fx->storage)
                    {
                        auto detailedMode = Surge::Storage::getUserDefaultValue(
                            fx->storage, Surge::Storage::HighPrecisionReadouts, 0);

                        fx->airwin->displayPrecision = (detailedMode ? 6 : 2);
                    }

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
