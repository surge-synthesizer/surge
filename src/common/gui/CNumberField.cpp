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

#include "CNumberField.h"
#include "SurgeStorage.h"
#include "UserDefaults.h"
#include <string>
#include <math.h>
#include "unitconversion.h"
#include <iostream>
#include "SkinColors.h"
#include "CScalableBitmap.h"
#include "Parameter.h"
#include "StringOps.h"

using namespace VSTGUI;

const int width = 62, margin = 6, height = 8, vmargin = 1;
extern CFontRef displayFont;

using namespace std;

//-------------------------------------------------------------------------------------------------------
// Strings Conversion
//-------------------------------------------------------------------------------------------------------
void dB2string(float value, char *text)
{
    if (value <= 0)
        strxcpy(text, "-inf", TXT_SIZE);
    else
        snprintf(text, TXT_SIZE, "%.1fdB", (float)(20. * log10(value)));
}

void envelopetime(float value, char *text)
{
    if ((value * value * value * 30000.f) > 999.9f)
        snprintf(text, TXT_SIZE, "%.2fs", (float)30 * value * value * value);
    else
        snprintf(text, TXT_SIZE, "%.0fms", (float)30000 * value * value * value);
}

void unit_prefix(float value, char *text, bool allow_milli = true, bool allow_kilo = true)
{
    char prefix = 0;
    if (allow_kilo && (value >= 1000.f))
    {
        value *= 0.001f;
        prefix = 'k';
    }
    else if (allow_milli && (value < 1.f))
    {
        value *= 1000.f;
        prefix = 'm';
    }

    if (value >= 100.f)
        snprintf(text, TXT_SIZE, "%.1f %c", value, prefix);
    else if (value >= 10.f)
        snprintf(text, TXT_SIZE, "%.2f %c", value, prefix);
    else
        snprintf(text, TXT_SIZE, "%.3f %c", value, prefix);
    return;
}

CNumberField::CNumberField(const CRect &size, IControlListener *listener, long tag,
                           CBitmap *pBackground, SurgeStorage *storage)
    : CControl(size, listener, tag, pBackground), Surge::UI::CursorControlAdapter<CNumberField>(
                                                      storage)
{
    i_value = 60;
    controlmode = cm_integer;
    i_min = 0;
    i_max = 127;
    i_stepsize = 1;
    i_default = 0;
    f_min = 0.0f;
    f_max = 1.0f;
    f_movespeed = 0.01;
    value = 0.0f;
    drawsize = size;
    i_poly = 0;
    this->storage = storage;
}

CNumberField::~CNumberField() {}

void CNumberField::setControlMode(int mode)
{
    controlmode = mode;

    switch (mode)
    {
    case cm_percent_bipolar:
        setFloatMin(-1.f);
        setFloatMax(1.f);
        setDefaultValue(0.0f);
        f_movespeed = 0.01f;
        break;
    case cm_frequency1hz:
        setFloatMin(-10);
        setFloatMax(5);
        setDefaultValue(0);
        f_movespeed = 0.04;
        break;
    case cm_lag:
        setFloatMin(-10);
        setFloatMax(5);
        setDefaultValue(0);
        f_movespeed = 0.1;
        break;
    case cm_time1s:
        setFloatMin(-10);
        setFloatMax(10);
        setDefaultValue(0);
        f_movespeed = 0.1;
        break;
    case cm_decibel:
        setFloatMin(-144);
        setFloatMax(24);
        setDefaultValue(0);
        f_movespeed = 0.15;
        break;
    case cm_mod_decibel:
        setFloatMin(-144);
        setFloatMax(144);
        setDefaultValue(0);
        f_movespeed = 0.15;
        break;
    case cm_mod_time:
        setFloatMin(-8);
        setFloatMax(8);
        setDefaultValue(0);
        f_movespeed = 0.01;
        break;
    case cm_mod_pitch:
        setFloatMin(-128);
        setFloatMax(128);
        setDefaultValue(0);
        f_movespeed = 0.1;
        break;
    case cm_mod_freq:
        setFloatMin(-12);
        setFloatMax(12);
        setDefaultValue(0);
        f_movespeed = 0.01;
        break;
    case cm_eq_bandwidth:
        setFloatMin(0.0001);
        setFloatMax(20);
        setDefaultValue(1);
        f_movespeed = 0.01;
        break;
    case cm_mod_percent:
        setFloatMin(-32);
        setFloatMax(32);
        setDefaultValue(0);
        f_movespeed = 0.01;
        break;
    case cm_stereowidth:
        setFloatMin(0);
        setFloatMax(4);
        setDefaultValue(1);
        f_movespeed = 0.01;
        break;
    case cm_frequency_audible:
        setFloatMin(-7);
        setFloatMax(6.0);
        setDefaultValue(0);
        f_movespeed = 0.01;
        break;
    case cm_frequency_samplerate:
        setFloatMin(-5);
        setFloatMax(7.5);
        setDefaultValue(0);
        f_movespeed = 0.01;
        break;
    case cm_frequency_khz:
        setFloatMin(0);
        setFloatMax(20);
        setDefaultValue(0);
        f_movespeed = 0.1;
        break;
    case cm_frequency_hz_bi:
        setFloatMin(-20000);
        setFloatMax(20000);
        setDefaultValue(0);
        f_movespeed = 0.1;
        break;
    case cm_frequency_khz_bi:
        setFloatMin(-20);
        setFloatMax(20);
        setDefaultValue(0);
        f_movespeed = 0.1;
        break;
    case cm_polyphony:
        setIntMin(2);
        setIntMax(64);
        setDefaultValue(8);
        break;
    case cm_pbdepth:
        setIntMin(0);
        setIntMax(24);
        setDefaultValue(2);
        break;
    case cm_envshape:
        setFloatMin(-10.f);
        setFloatMax(20.f);
        setDefaultValue(0.f);
        f_movespeed = 0.02f;
        break;
    case cm_osccount:
        setFloatMin(0.5f);
        setFloatMax(16.5f);
        f_movespeed = 0.1f;
        break;
    case cm_count4:
        setFloatMin(0.5f);
        setFloatMax(4.5f);
        f_movespeed = 0.1f;
        break;
    case cm_noyes:
        setIntMin(0);
        setIntMax(1);
        setIntDefaultValue(0);
        break;
    case cm_mseg_snap_h:
    case cm_mseg_snap_v:
        setIntMin(1);
        setIntMax(100);
        if (mode == cm_mseg_snap_h)
            setIntDefaultValue(10);
        else
            setIntDefaultValue(4);
        break;
    default:
        setFloatMin(0.f);
        setFloatMax(1.f);
        setDefaultValue(0.5f);
        f_movespeed = 0.01f;
        break;
    };
    setDirty();
}
//------------------------------------------------------------------------
void CNumberField::setValue(float val)
{
    CControl::setValue(val);
    i_value = Parameter::intUnscaledFromFloat(val, i_max, i_min);
    setDirty();
}

//------------------------------------------------------------------------
void CNumberField::draw(CDrawContext *pContext)
{
    auto colorName = skin->propertyValue(skinControl, Surge::Skin::Component::TEXT_COLOR,
                                         Colors::NumberField::Text.name);
    auto hoverColorName = skin->propertyValue(skinControl, Surge::Skin::Component::TEXT_HOVER_COLOR,
                                              Colors::NumberField::TextHover.name);

    auto fontColor = kRedCColor;
    if (hovered)
        fontColor = skin->getColor(hoverColorName);
    else
        fontColor = skin->getColor(colorName);

    // cache this of course
    if (!triedToLoadBg)
    {
        bg = skin->backgroundBitmapForControl(skinControl, associatedBitmapStore);
        hoverBg = skin->hoverBitmapOverlayForBackgroundBitmap(
            skinControl, bg, associatedBitmapStore, Surge::UI::Skin::HOVER);
        triedToLoadBg = true;
    }

    if (bg)
    {
        bg->draw(pContext, getViewSize(), CPoint(), 0xff);
    }

    if (hovered && hoverBg)
    {
        hoverBg->draw(pContext, getViewSize(), CPoint(), 0xff);
    }

    if (!(bg || hoverBg))
    {
        pContext->setFrameColor(kRedCColor);
        pContext->setFillColor(VSTGUI::CColor(100, 100, 200));
        pContext->drawRect(getViewSize(), kDrawFilledAndStroked);
    }

#define THE_TEXT_SIZE TXT_SIZE
    char the_text[THE_TEXT_SIZE];
    switch (controlmode)
    {
    case cm_int_menu:
        snprintf(the_text, THE_TEXT_SIZE, "-");
        break;
    case cm_integer:
        snprintf(the_text, THE_TEXT_SIZE, "%i", i_value);
        break;
    case cm_osccount:
        snprintf(the_text, THE_TEXT_SIZE, "%i", max(1, min(16, (int)value)));
        break;
    case cm_pbdepth:
        snprintf(the_text, THE_TEXT_SIZE, "%i", i_value);
        break;
    case cm_count4:
        snprintf(the_text, THE_TEXT_SIZE, "%i", max(1, min(4, (int)value)));
        break;
    case cm_polyphony:
        snprintf(the_text, THE_TEXT_SIZE, "%i / %i", i_poly, i_value);
        break;
    case cm_midichannel_from_127:
    {
        int mc = i_value / 8 + 1;
        snprintf(the_text, THE_TEXT_SIZE, "Ch %i", mc);
    }
    break;
    case cm_notename:
    {
        int oct_offset = 1;
        if (storage)
            oct_offset = Surge::Storage::getUserDefaultValue(storage, "middleC", 1);
        char notename[16];
        snprintf(the_text, THE_TEXT_SIZE, "%s", get_notename(notename, i_value, oct_offset));
    }
    break;
    case cm_envshape:
        snprintf(the_text, THE_TEXT_SIZE, "%.1f", value);
        break;
    case cm_float:
        snprintf(the_text, THE_TEXT_SIZE, "%.2f", value);
        break;
    case cm_percent:
        snprintf(the_text, THE_TEXT_SIZE, "%.1f%c", value * 100, '%');
        break;
    case cm_mod_percent:
    case cm_percent_bipolar:
        snprintf(the_text, THE_TEXT_SIZE, "%+.1f%c", value * 100, '%');
        break;
    case cm_stereowidth:
    {
        if (value == 0)
            snprintf(the_text, THE_TEXT_SIZE, "mono");
        else
            snprintf(the_text, THE_TEXT_SIZE, "%.1f%c", value * 100, '%');
        break;
    }
    case cm_mod_time:
        snprintf(the_text, THE_TEXT_SIZE, "%+.1fto", value);
        break;
    case cm_decibel:
        snprintf(the_text, THE_TEXT_SIZE, "%.1fdB", value);
        break;
    case cm_mod_decibel:
        snprintf(the_text, THE_TEXT_SIZE, "%+.1fdB", value);
        break;
    case cm_mod_pitch:
        snprintf(the_text, THE_TEXT_SIZE, "%+.1fst", value);
        break;
    case cm_mod_freq:
    case cm_eq_bandwidth:
        snprintf(the_text, THE_TEXT_SIZE, "%.2foct", value);
        break;
    case cm_frequency_khz:
        snprintf(the_text, THE_TEXT_SIZE, "%.2f kHz", value);
        break;
    case cm_frequency_hz_bi:
        snprintf(the_text, THE_TEXT_SIZE, "%+.2f Hz", value);
        break;
    case cm_frequency_khz_bi:
        snprintf(the_text, THE_TEXT_SIZE, "%+.2f kHz", value);
        break;
    case cm_envelopetime:
        envelopetime(value, the_text);
        break;
    case cm_decibel_squared:
        dB2string(value * value, the_text);
        break;
    case cm_lforate:
    {
        float rate = lfo_phaseincrement(44100 / 32, value);
        snprintf(the_text, THE_TEXT_SIZE, "%.3f Hz", rate);
    }
    break;
    case cm_frequency_audible:
    case cm_frequency_samplerate:
    {
        float freq = 440.f * powf(2, value);
        if (freq > 1000)
            snprintf(the_text, THE_TEXT_SIZE, "%.2f kHz", freq * 0.001f);
        else
            snprintf(the_text, THE_TEXT_SIZE, "%.1f Hz", freq);
        break;
    }
    case cm_frequency20_20k:
    {
        float freq = 20 * powf(10, 3 * value);
        if (freq > 1000)
            snprintf(the_text, THE_TEXT_SIZE, "%.2f kHz", freq * 0.001f);
        else
            snprintf(the_text, THE_TEXT_SIZE, "%.1f Hz", freq);
        break;
    }
    case cm_frequency50_50k:
    {
        float freq = 50 * powf(10, 3 * value);
        if (freq > 1000)
            snprintf(the_text, THE_TEXT_SIZE, "%.2f kHz", freq * 0.001f);
        else
            snprintf(the_text, THE_TEXT_SIZE, "%.1f Hz", freq);
        break;
    }
    case cm_lag:
    {
        if (value == -10)
            snprintf(the_text, THE_TEXT_SIZE, "sum");
        else
        {
            float t = powf(2, value);
            char tmp_text[TXT_SIZE];
            unit_prefix(t, tmp_text, true, false);
            snprintf(the_text, THE_TEXT_SIZE, "%ss", tmp_text);
        }
        break;
    }
    case cm_bitdepth_16:
    {
        snprintf(the_text, THE_TEXT_SIZE, "%.1f bits", value * 16.f);
        break;
    }
    case cm_frequency0_2k:
    {
        float freq = 2000 * value;
        if (freq > 1000)
            snprintf(the_text, THE_TEXT_SIZE, "%.2f kHz", freq * 0.001f);
        else
            snprintf(the_text, THE_TEXT_SIZE, "%.1f Hz", freq);
        break;
    }
    case cm_octaves3:
        snprintf(the_text, THE_TEXT_SIZE, "%.1f oct", value * 3.f);
        break;
    case cm_decibelboost12:
        snprintf(the_text, THE_TEXT_SIZE, "+%.1f dB", value * 12.f);
        break;
    case cm_midichannel:
        if (i_value < 16)
            snprintf(the_text, THE_TEXT_SIZE, "%i", i_value + 1);
        else if (i_value == 16)
            snprintf(the_text, THE_TEXT_SIZE, "omni");
        else
            snprintf(the_text, THE_TEXT_SIZE, "-");
        break;
    case cm_mutegroup:
        if (i_value == 0)
            snprintf(the_text, THE_TEXT_SIZE, "off");
        else
            snprintf(the_text, THE_TEXT_SIZE, "%i", i_value);
        break;
    case cm_frequency1hz:
    {
        float freq = powf(2, value);
        char tmp_text[TXT_SIZE];
        unit_prefix(freq, tmp_text, false, false);
        snprintf(the_text, THE_TEXT_SIZE, "%sHz", tmp_text);
        break;
    }
    case cm_time1s:
    {
        float t = powf(2, value);
        char tmp_text[TXT_SIZE];
        unit_prefix(t, tmp_text, true, false);
        snprintf(the_text, THE_TEXT_SIZE, "%ss", tmp_text);
        break;
    }
    case cm_noyes:
        if (i_value)
            snprintf(the_text, THE_TEXT_SIZE, "yes");
        else
            snprintf(the_text, THE_TEXT_SIZE, "no");
        break;
    case cm_mseg_snap_h:
    case cm_mseg_snap_v:
        snprintf(the_text, THE_TEXT_SIZE, "%i", i_value);
        break;
    case cm_none:
        snprintf(the_text, THE_TEXT_SIZE, "-");
        break;
    }

    pContext->setFont(displayFont);
    pContext->setFontColor(fontColor);

    pContext->drawString(the_text, getViewSize(), kCenterText, true);

    setDirty(false);
#undef THE_TEXT_SIZE
}

//------------------------------------------------------------------------
void CNumberField::bounceValue()
{
    if (i_value > i_max)
        i_value = i_max;
    if (i_value < i_min)
        i_value = i_min;

    if (value > f_max)
        value = f_max;
    if (value < f_min)
        value = f_min;
}

/*bool CParamEdit::onWheel (CDrawContext *pContext, const CPoint &wh, float distance)
{
        CPoint where(wh.x,wh.y);
        where.offset(-size.x,-size.y);

        if (distance>0)
        {
                i_value++;
        }
        else
        {
                i_value--;
        }

        bounceValue();

        if (listener)
                listener->valueChanged (pContext, this);
        setDirty();

        return true;
}*/

enum
{
    cs_null = 0,
    cs_drag,
};

CMouseEventResult CNumberField::onMouseDown(CPoint &where, const CButtonState &buttons)
{
    if (controlmode == cm_int_menu)
        return kMouseDownEventHandledButDontNeedMovedOrUpEvents;

    if (controlmode == cm_noyes)
    {
        i_value = !i_value;
        if (listener)
            listener->valueChanged(this);

        setDirty();
        return kMouseDownEventHandledButDontNeedMovedOrUpEvents;
    }

    if (listener && buttons & (kAlt | kRButton | kMButton | kShift | kControl | kApple))
    {
        if (listener->controlModifierClicked(this, buttons) != 0)
        {
            setDirty();
            return kMouseDownEventHandledButDontNeedMovedOrUpEvents;
        }
    }

    if (buttons & kDoubleClick)
    {
        if (listener)
            listener->controlModifierClicked(this, buttons);
        {
            setDirty();
            return kMouseDownEventHandledButDontNeedMovedOrUpEvents;
        }
    }

    if ((buttons & kLButton) && (drawsize.pointInside(where)))
    {
        enqueueCursorHide = true;
        controlstate = cs_drag;
        lastmousepos = where;
        startmousepos = where;
        f_min = 0.f;
        f_max = 1.f;
        value = ((float)(i_value - i_min)) / ((float)(i_max - i_min));
        beginEdit();
        return kMouseEventHandled;
    }

    return kMouseDownEventHandledButDontNeedMovedOrUpEvents;
}
CMouseEventResult CNumberField::onMouseUp(CPoint &where, const CButtonState &buttons)
{
    enqueueCursorHide = false;
    if (controlstate)
    {
        endEdit();
        endCursorHide();
        controlstate = cs_null;
    }
    return kMouseEventHandled;
}
CMouseEventResult CNumberField::onMouseMoved(CPoint &where, const CButtonState &buttons)
{
    if ((controlstate == cs_drag) && (buttons & kLButton))
    {
        if (enqueueCursorHide)
        {
            startCursorHide(where);
            enqueueCursorHide = false;
        }
        float dx = where.x - lastmousepos.x;
        float dy = where.y - lastmousepos.y;

        float delta =
            dx -
            dy; // for now lets try this. Remenber y 'up' in logical space is 'down' in pixel space

        float odx = where.x - startmousepos.x;
        float ody = where.y - startmousepos.y;
        float odelt = sqrt(odx * odx + ody * ody);

        if (odelt > 10)
        {
            if (resetToShowLocation())
                lastmousepos = startmousepos;
            else
                lastmousepos = where;
        }
        else
        {
            lastmousepos = where;
        }

        if (buttons & kShift)
            delta *= 0.1;
        if (buttons & kRButton)
            delta *= 0.1;

        // For notename, this is all way too fast #1436
        if (controlmode == cm_notename)
        {
            delta = delta * 0.4;
        }

        value += delta * 0.01;
        i_value = Parameter::intUnscaledFromFloat(value, i_max, i_min);

        bounceValue();
        invalid();
        if (listener)
            listener->valueChanged(this);
    }
    return kMouseEventHandled;
}

bool CNumberField::onWheel(const CPoint &where, const float &distance, const CButtonState &buttons)
{
    beginEdit();
    double mouseFactor = 1;

    if (controlmode == cm_midichannel_from_127)
        mouseFactor = 7.5;

    if (buttons & kControl)
        value += distance * 0.01;
    else
        value += distance / (i_max - i_min) * mouseFactor;

    i_value = Parameter::intUnscaledFromFloat(value, i_max, i_min);
    bounceValue();
    invalid();
    setDirty();
    if (isDirty() && listener)
        listener->valueChanged(this);

    endEdit();
    return true;
}
