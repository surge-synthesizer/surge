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

#ifndef SURGE_XT_JUCEHELPERS_H
#define SURGE_XT_JUCEHELPERS_H

#include "juce_gui_basics/juce_gui_basics.h"
#include "SurgeGUICallbackInterfaces.h"
#include <filesystem/import.h>

namespace Surge
{
namespace GUI
{
template <typename T>
inline std::function<void(int)> makeAsyncCallback(T *that, std::function<void(T *, int)> cb)
{
    return [safethat = juce::Component::SafePointer<T>(that), cb](int x) {
        if (safethat)
            cb(safethat, x);
    };
}

template <typename T>
inline std::function<void()> makeSafeCallback(T *that, std::function<void(T *)> cb)
{
    return [safethat = juce::Component::SafePointer<T>(that), cb]() {
        if (safethat)
            cb(safethat);
    };
}

template <typename T> inline std::function<void(int)> makeEndHoverCallback(T *that)
{
    return [safethat = juce::Component::SafePointer<T>(that)](int x) {
        if (safethat)
        {
            safethat->endHover();
        }
    };
}

template <>
inline std::function<void(int)> makeEndHoverCallback(Surge::GUI::IComponentTagValue *that)
{
    if (!that)
        return [](int) {};

    that->stuckHoverOn();

    return
        [safethat = juce::Component::SafePointer<juce::Component>(that->asJuceComponent())](int x) {
            if (safethat)
            {
                auto igtv = dynamic_cast<Surge::GUI::IComponentTagValue *>(safethat.getComponent());
                if (igtv)
                {
                    igtv->stuckHoverOff();
                    igtv->endHover();
                }
            }
        };
}
struct WheelAccumulationHelper
{
    float accum{0};

    int accumulate(const juce::MouseWheelDetails &wheel, bool X = true, bool Y = true)
    {
        accum += X * wheel.deltaX - (wheel.isReversed ? 1 : -1) * Y * wheel.deltaY;
        // This is calibrated to be reasonable on a Mac but I'm still not sure of the units
        // On my MBP I get deltas which are 0.0019 all the time.

        float accumLimit = 0.08;

        if (accum > accumLimit || accum < -accumLimit)
        {
            int mul = -1;

            if (accum > 0)
            {
                mul = 1;
            }

            accum = 0;

            return mul;
        }

        return 0;
    }
};

inline void addMenuWithShortcut(juce::PopupMenu &m, const std::string &lab,
                                const std::string &shortcut, bool enabled, bool ticked,
                                std::function<void()> action)
{
    auto i = juce::PopupMenu::Item(lab)
                 .setAction(std::move(action))
                 .setEnabled(enabled)
                 .setTicked(ticked);

    i.shortcutKeyDescription = shortcut;
    m.addItem(i);
}

inline void addMenuWithShortcut(juce::PopupMenu &m, const std::string &lab,
                                const std::string &shortcut, std::function<void()> action)
{
    addMenuWithShortcut(m, lab, shortcut, true, false, action);
}

inline void addRevealFile(juce::PopupMenu &m, const fs::path &p)
{
#if LINUX
    return;
#else
    try
    {
        if (fs::exists(p) && fs::is_regular_file(p))
        {
            auto f = juce::File(p.u8string());
#if MAC
            std::string s = "Reveal in Finder...";
#else
            std::string s = "Open in Explorer...";
#endif
            m.addItem(s, [f]() { f.revealToUser(); });
        }
    }
    catch (fs::filesystem_error &)
    {
    }
#endif
}

} // namespace GUI
} // namespace Surge

#endif // SURGE_XT_JUCEHELPERS_H
