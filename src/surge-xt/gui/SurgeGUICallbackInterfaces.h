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

#ifndef SURGE_XT_SURGEGUICALLBACKINTERFACES_H
#define SURGE_XT_SURGEGUICALLBACKINTERFACES_H

#include "juce_gui_basics/juce_gui_basics.h"

namespace Surge
{
namespace GUI
{
struct Hoverable
{
    virtual void startHover(const juce::Point<float> &) {}
    virtual void endHover() {}
    virtual bool isCurrentlyHovered() { return false; }
};

struct IComponentTagValue : public Hoverable
{
    virtual uint32_t getTag() const = 0;
    virtual float getValue() const = 0;
    virtual void setValue(float) = 0;

    bool stuckHover{false};
    virtual void stuckHoverOn() { stuckHover = true; }
    virtual void stuckHoverOff() { stuckHover = false; }

    juce::Component *asJuceComponent()
    {
        auto r = dynamic_cast<juce::Component *>(this);
        if (r)
            return r;
        jassert(false);
        return nullptr;
    }

    struct Listener
    {
        virtual void valueChanged(IComponentTagValue *p) = 0;
        virtual int32_t controlModifierClicked(IComponentTagValue *p,
                                               const juce::ModifierKeys &mods,
                                               bool isDoubleClickEvent)
        {
            return false;
        }

        virtual void controlBeginEdit(IComponentTagValue *control){};
        virtual void controlEndEdit(IComponentTagValue *control){};
    };
};
} // namespace GUI
} // namespace Surge
#endif // SURGE_XT_SURGEGUICALLBACKINTERFACES_H
