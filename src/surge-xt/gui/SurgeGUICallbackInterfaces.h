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

#ifndef SURGE_SRC_SURGE_XT_GUI_SURGEGUICALLBACKINTERFACES_H
#define SURGE_SRC_SURGE_XT_GUI_SURGEGUICALLBACKINTERFACES_H

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
