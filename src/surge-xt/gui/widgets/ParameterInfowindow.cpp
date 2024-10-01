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

#include "ParameterInfowindow.h"
#include "RuntimeFont.h"

namespace Surge
{
namespace Widgets
{
ParameterInfowindow::ParameterInfowindow() { setInterceptsMouseClicks(false, false); }
ParameterInfowindow::~ParameterInfowindow() = default;

void ParameterInfowindow::paint(juce::Graphics &g)
{
    // Save state since I want to set an overall alpha
    juce::Graphics::ScopedSaveState gs(g);
    float opacity = 1.f;

    if (countdownFade >= 0)
    {
        opacity = 1.f * countdownFade / fadeOutOver;
    }
    if (countdownFadeIn >= 0)
    {
        opacity = 1.f * (fadeInOver - countdownFadeIn) / fadeInOver;
    }

    auto frameCol = skin->getColor(Colors::InfoWindow::Border);
    auto bgCol = skin->getColor(Colors::InfoWindow::Background);
    auto txtCol = skin->getColor(Colors::InfoWindow::Text);
    auto mpCol = skin->getColor(Colors::InfoWindow::Modulation::Positive);
    auto mnCol = skin->getColor(Colors::InfoWindow::Modulation::Negative);
    auto mpValCol = skin->getColor(Colors::InfoWindow::Modulation::ValuePositive);
    auto mnValCol = skin->getColor(Colors::InfoWindow::Modulation::ValueNegative);

    g.setColour(frameCol);
    g.setOpacity(opacity);
    g.fillRect(getLocalBounds());

    g.setColour(bgCol);
    g.setOpacity(opacity);
    g.fillRect(getLocalBounds().reduced(1));

    g.setColour(txtCol);
    g.setOpacity(opacity);
    font = skin->fontManager->displayFont;
    g.setFont(font);

    if (name.empty())
    {
        // We are not in a modulation mode. So display is |alt .... disp|
        auto r = getLocalBounds().reduced(3, 1);

        g.drawText(display, r, juce::Justification::centredRight);
        g.drawText(displayAlt, r, juce::Justification::centredLeft);
    }
    else
    {
        if (hasExt && hasModDisInf)
        {
            // We have the full structured modulation info and should do 3 line display
            auto ro = getLocalBounds().reduced(3, 1);
            auto rt = ro.withTrimmedBottom(2 * ro.getHeight() / 3);
            auto rm = ro.withTrimmedTop(ro.getHeight() / 3).withTrimmedBottom(ro.getHeight() / 3);
            auto rb = ro.withTrimmedTop(2 * ro.getHeight() / 3);

            // name
            g.drawText(name, rt, juce::Justification::centredLeft);

            // val with changes
            auto valalign = juce::Justification::centred;

            if (mdiws.dvalminus.size() == 0 && mdiws.valminus.size() == 0)
            {
                valalign = juce::Justification::centredLeft;
            }

            g.drawText(mdiws.val, rm, valalign);
            g.setColour(mpCol);
            g.setOpacity(opacity);
            g.drawText(mdiws.dvalplus, rm, juce::Justification::centredRight);
            g.setColour(mnCol);
            g.setOpacity(opacity);
            g.drawText(mdiws.dvalminus, rm, juce::Justification::centredLeft);

            g.setColour(mpValCol);
            g.setOpacity(opacity);
            g.drawText(mdiws.valplus, rb, juce::Justification::centredRight);
            g.setColour(mnValCol);
            g.setOpacity(opacity);
            g.drawText(mdiws.valminus, rb, juce::Justification::centredLeft);
        }
        else
        {
            // We have the abbreviated 2 line modulation display
            auto ro = getLocalBounds().reduced(3, 1);
            auto rt = ro.withTrimmedBottom(ro.getHeight() / 2);
            auto rb = ro.withTrimmedTop(ro.getHeight() / 2);

            g.drawText(name, rt, juce::Justification::centredLeft);
            g.drawText(display, rb, juce::Justification::centredLeft);
        }
    }
}

void ParameterInfowindow::setBoundsToAccompany(const juce::Rectangle<int> &controlRect,
                                               const juce::Rectangle<int> &parentRect)
{
    int desiredHeight = font.getHeight() + 5;
    int desiredWidth = 141;

    if (name.empty())
    {
        auto sl1 = SST_STRING_WIDTH_INT(font, display);
        auto sl2 = SST_STRING_WIDTH_INT(font, displayAlt);
        auto pad = SST_STRING_WIDTH_INT(font, "  ");
        desiredWidth = std::max(sl1 + sl2 + pad, desiredWidth);
    }
    else
    {
        int lheight = 0;

        if (hasExt && hasModDisInf)
        {
            lheight = font.getHeight() * 3 + 11;
            // row 2
            auto r1l = SST_STRING_WIDTH_INT(font, name);
            auto r2l = SST_STRING_WIDTH_INT(font, mdiws.dvalminus + "  " + mdiws.val + "  " +
                                                      mdiws.dvalplus);
            auto r3l = SST_STRING_WIDTH_INT(font, mdiws.valminus + "  " + mdiws.valplus);
            desiredWidth = std::max(std::max(std::max(r1l, r2l), r3l) + 8, desiredWidth);
        }
        else
        {
            auto sln = SST_STRING_WIDTH_INT(font, name);
            auto sl1 = SST_STRING_WIDTH_INT(font, display);
            desiredWidth = std::max(std::max(sln, sl1) + 8, desiredWidth);
            lheight = font.getHeight() * 2 + 9;
        }

        desiredHeight = std::max(desiredHeight, lheight);
    }

    auto r = juce::Rectangle<int>(0, 0, desiredWidth, desiredHeight)
                 .withX(controlRect.getX())
                 .withY(controlRect.getY() + controlRect.getHeight());

    // OK so now we are well sized. The only question is are we inside?
    if (!parentRect.contains(r))
    {
        auto h = r.getHeight(), w = r.getWidth();
        if (r.getBottom() > parentRect.getBottom())
        {
            // push up
            r = r.withY(controlRect.getY() - h).withHeight(h);
        }
        else
        {
            jassert(false);
        }
        if (r.getRight() > parentRect.getRight())
        {
            // push left
            r = r.withX(controlRect.getX() - w).withWidth(w);
        }
    }

    setBounds(r);
}

void ParameterInfowindow::doHide(int afterIdles)
{
    if (afterIdles > 0)
    {
        countdownHide = afterIdles;
        countdownFade = -1;
    }
    else
    {
        countdownHide = -1;
        countdownFade = fadeOutOver;
    }
}

void ParameterInfowindow::idle()
{
    if (countdownHide < 0 && countdownFade < 0 && countdownFadeIn < 0)
    {
        return;
    }

    if (countdownHide == 0)
    {
        doHide();
    }

    if (countdownHide > 0)
    {
        countdownHide--;
    }

    if (countdownFade == 0)
    {
        setVisible(false);
        countdownFade = -1;
    }

    if (countdownFade > 0)
    {
        countdownFade--;
        repaint();
    }

    if (countdownFadeIn >= 0)
    {
        countdownFadeIn--;
        repaint();
    }
}
} // namespace Widgets
} // namespace Surge
