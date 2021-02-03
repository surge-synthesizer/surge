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

#pragma once
#if ESCAPE_FROM_VSTGUI
#include <efvg/escape_from_vstgui.h>
#else
#include <vstgui/vstgui.h>
#endif

class CTextButtonWithHover : public VSTGUI::CTextButton
{
  public:
    template <typename T> struct Optional
    {
        T item = T();
        bool isSet = false;
    };

    template <typename T> struct OptionalForget
    {
        ~OptionalForget()
        {
#if TARGET_JUCE_UI
            if (isSet && item)
            {
                // item->forget();
                item = nullptr;
            }
#endif
        }
        T item = T();
        bool isSet = false;
    };

    CTextButtonWithHover(const VSTGUI::CRect &r, VSTGUI::IControlListener *l, int32_t tag,
                         VSTGUI::UTF8String lab)
        : CTextButton(r, l, tag, lab)
    {
    }
    ~CTextButtonWithHover()
    {
#if TARGET_JUCE_UI
        if (getGradient())
        {
            // getGradient()->forget();
            setGradient(nullptr);
        }
        if (getGradientHighlighted())
        {
            // getGradientHighlighted()->forget();
            setGradientHighlighted(nullptr);
        }
#endif
    }
    VSTGUI::CMouseEventResult onMouseEntered(VSTGUI::CPoint &where,
                                             const VSTGUI::CButtonState &buttons) override;
    void draw(VSTGUI::CDrawContext *context) override;
    VSTGUI::CMouseEventResult onMouseExited(VSTGUI::CPoint &where,
                                            const VSTGUI::CButtonState &buttons) override;

    /*
     * Macros are still the answer sometimes.
     */
#define ADD_HOVER(m, T)                                                                            \
    OptionalForget<T> hc_##m;                                                                      \
    void setHover##m(T c)                                                                          \
    {                                                                                              \
        hc_##m.item = c;                                                                           \
        hc_##m.isSet = true;                                                                       \
    }

#define ADD_HOVER_CR(m, T)                                                                         \
    Optional<T> hc_##m;                                                                            \
    void setHover##m(const T &c)                                                                   \
    {                                                                                              \
        hc_##m.item = c;                                                                           \
        hc_##m.isSet = true;                                                                       \
    }

    ADD_HOVER(Gradient, VSTGUI::CGradient *);
    ADD_HOVER_CR(FrameColor, VSTGUI::CColor);
    ADD_HOVER_CR(TextColor, VSTGUI::CColor);
    ADD_HOVER(GradientHighlighted, VSTGUI::CGradient *);
    ADD_HOVER_CR(FrameColorHighlighted, VSTGUI::CColor);
    ADD_HOVER_CR(TextColorHighlighted, VSTGUI::CColor);

#undef ADD_HOVER

  private:
    bool isHovered = false;
    CLASS_METHODS(CTextButtonWithHover, VSTGUI::CTextButton);
};
