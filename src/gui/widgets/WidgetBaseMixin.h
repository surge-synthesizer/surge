/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2021 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

#ifndef SURGE_XT_WIDGETBASEMIXIN_H
#define SURGE_XT_WIDGETBASEMIXIN_H

#include "efvg/escape_from_vstgui.h"
#include "SkinSupport.h"
#include <unordered_set>

namespace Surge
{
namespace Widgets
{
template <typename T>
struct WidgetBaseMixin : public Surge::GUI::SkinConsumingComponent,
                         public VSTGUI::BaseViewFunctions,
                         public VSTGUI::CControlValueInterface
{
    inline T *asT() { return static_cast<T *>(this); }

    void invalid() override { asT()->repaint(); }

    VSTGUI::CRect getControlViewSize() override { return VSTGUI::CRect(asT()->getBounds()); }

    VSTGUI::CCoord getControlHeight() override { return asT()->getHeight(); }
    uint32_t tag{0};
    void setTag(uint32_t t) { tag = t; }
    uint32_t getTag() const override { return tag; }

    std::unordered_set<VSTGUI::IControlListener *> listeners;
    void addListener(VSTGUI::IControlListener *t) { listeners.insert(t); }
    void notifyValueChanged()
    {
        for (auto t : listeners)
            t->valueChanged(this);
    }
    void notifyControlModifierClicked(const juce::ModifierKeys &k)
    {
        VSTGUI::CButtonState bs(k);
        for (auto t : listeners)
            t->controlModifierClicked(this, bs);
    }

    template <typename U> U *firstListenerOfType()
    {
        for (auto u : listeners)
        {
            auto q = dynamic_cast<U *>(u);
            if (q)
                return q;
        }
        return nullptr;
    }
};
} // namespace Widgets
} // namespace Surge
#endif // SURGE_XT_WIDGETBASEMIXIN_H
