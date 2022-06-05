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

#ifndef SURGE_XT_SURGEJUCELOOKANDFEEL_H
#define SURGE_XT_SURGEJUCELOOKANDFEEL_H

#include "juce_gui_basics/juce_gui_basics.h"
#include "SkinSupport.h"

class SurgeJUCELookAndFeel : public juce::LookAndFeel_V4, public Surge::GUI::SkinConsumingComponent
{
  public:
    SurgeJUCELookAndFeel(SurgeStorage *s) : storage(s) {}
    void drawLabel(juce::Graphics &graphics, juce::Label &label) override;
    void drawTextEditorOutline(juce::Graphics &graphics, int width, int height,
                               juce::TextEditor &editor) override;

    juce::Button *createDocumentWindowButton(int i) override;
    void drawDocumentWindowTitleBar(juce::DocumentWindow &window, juce::Graphics &graphics, int i,
                                    int i1, int i2, int i3, const juce::Image *image,
                                    bool b) override;

    int getTabButtonBestWidth(juce::TabBarButton &, int d) override;
    void drawTabButton(juce::TabBarButton &button, juce::Graphics &g, bool isMouseOver,
                       bool isMouseDown) override;

    void onSkinChanged() override;
    SurgeStorage *storage{nullptr};

    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackgroundWithOptions(juce::Graphics &g, int w, int h,
                                            const juce::PopupMenu::Options &o) override;
    void drawPopupMenuItem(juce::Graphics &g, const juce::Rectangle<int> &area,
                           const bool isSeparator, const bool isActive, const bool isHighlighted,
                           const bool isTicked, const bool hasSubMenu, const juce::String &text,
                           const juce::String &shortcutKeyText, const juce::Drawable *icon,
                           const juce::Colour *const textColourToUse) override;
    void drawPopupMenuSectionHeaderWithOptions(juce::Graphics &graphics,
                                               const juce::Rectangle<int> &area,
                                               const juce::String &sectionName,
                                               const juce::PopupMenu::Options &options) override;

    void drawCornerResizer(juce::Graphics &g, int w, int h, bool, bool) override{};

    enum SurgeColourIds
    {
        componentBgStart = 0x3700001,
        componentBgEnd,

        topWindowBorderId,

        tempoBackgroundId,
        tempoLabelId,

        tempoTypeinBackgroundId,
        tempoTypeinBorderId,
        tempoTypeinHighlightId,
        tempoTypeinTextId,

        wheelBgId,
        wheelBorderId,
        wheelValueId,
    };

    int lastDark{-1};
    void updateDarkIfNeeded();
};
#endif // SURGE_XT_SURGEJUCELOOKANDFEEL_H
