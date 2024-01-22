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

#ifndef SURGE_SRC_SURGE_XT_GUI_SURGEJUCELOOKANDFEEL_H
#define SURGE_SRC_SURGE_XT_GUI_SURGEJUCELOOKANDFEEL_H

#include "juce_gui_basics/juce_gui_basics.h"
#include "SkinSupport.h"
#include <cassert>

class SurgeJUCELookAndFeel : public juce::LookAndFeel_V4, public Surge::GUI::SkinConsumingComponent
{
  public:
    SurgeJUCELookAndFeel() {}

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
    // SurgeStorage *storage{nullptr};
    std::set<SurgeStorage *> storagePointers;
    void addStorage(SurgeStorage *s) { storagePointers.insert(s); }
    void removeStorage(SurgeStorage *s) { storagePointers.erase(s); }
    SurgeStorage *storage()
    {
        assert(!storagePointers.empty());
        return *(storagePointers.begin());
    }
    bool hasStorage() { return !storagePointers.empty(); }

    juce::Font getPopupMenuFont() override;
    juce::Font getPopupMenuBoldFont();
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

    void drawToggleButton(juce::Graphics &g, juce::ToggleButton &b,
                          bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

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
