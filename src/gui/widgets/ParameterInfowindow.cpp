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

#include "ParameterInfowindow.h"
#include "RuntimeFont.h"

namespace Surge
{
namespace Widgets
{
ParameterInfowindow::ParameterInfowindow()
{
    setInterceptsMouseClicks(false, false);
    setFont(Surge::GUI::getFontManager()->displayFont);
}
ParameterInfowindow::~ParameterInfowindow() = default;

void ParameterInfowindow::paint(juce::Graphics &g)
{
    auto frameCol = skin->getColor(Colors::InfoWindow::Border);
    auto bgCol = skin->getColor(Colors::InfoWindow::Background);

    auto txtCol = skin->getColor(Colors::InfoWindow::Text);
    auto mpCol = skin->getColor(Colors::InfoWindow::Modulation::Positive);
    auto mnCol = skin->getColor(Colors::InfoWindow::Modulation::Negative);
    auto mpValCol = skin->getColor(Colors::InfoWindow::Modulation::ValuePositive);
    auto mnValCol = skin->getColor(Colors::InfoWindow::Modulation::ValueNegative);

    g.fillAll(bgCol);
    g.setColour(frameCol);
    g.drawRect(getLocalBounds());

    g.setColour(txtCol);
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
                valalign = juce::Justification::centredLeft;
            g.drawText(mdiws.val, rm, valalign);
            g.setColour(mpCol);
            g.drawText(mdiws.dvalplus, rm, juce::Justification::centredRight);
            g.setColour(mnCol);
            g.drawText(mdiws.dvalminus, rm, juce::Justification::centredLeft);

            g.setColour(mpValCol);
            g.drawText(mdiws.valplus, rb, juce::Justification::centredRight);
            g.setColour(mnValCol);
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
        auto sl1 = font.getStringWidth(display);
        auto sl2 = font.getStringWidth(displayAlt);
        auto pad = font.getStringWidth("  ");
        desiredWidth = std::max(sl1 + sl2 + pad, desiredWidth);
    }
    else
    {
        int lheight = 0;
        if (hasExt && hasModDisInf)
        {
            lheight = font.getHeight() * 3 + 11;
            // row 2
            auto r1l = font.getStringWidth(name);
            auto r2l =
                font.getStringWidth(mdiws.dvalminus + "  " + mdiws.val + "  " + mdiws.dvalplus);
            auto r3l = font.getStringWidth(mdiws.valminus + "  " + mdiws.valplus);
            desiredWidth = std::max(std::max(std::max(r1l, r2l), r3l) + 8, desiredWidth);
        }
        else
        {
            auto sln = font.getStringWidth(name);
            auto sl1 = font.getStringWidth(display);
            desiredWidth = std::max(std::max(sln, sl1) + 8, desiredWidth);
            lheight = font.getHeight() * 2 + 9;
        }
        desiredHeight = std::max(desiredHeight, lheight);
    }
    auto r = juce::Rectangle<int>(0, 0, desiredWidth, desiredHeight)
                 .withX(controlRect.getX())
                 .withY(controlRect.getY() + controlRect.getHeight());

    // OK so now we are well sized. The only question is are we inside
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
    }

    setBounds(r);
#if 0
// A heuristic
    auto ml = infowindow->getMaxLabelLen();
    auto iff = 148;
    // This is just empirical. It would be lovely to use the actual string width but that needs a
    // draw context of for these to be TextLabels so we can call sizeToFit
    if (ml > 24)
        iff += (ml - 24) * 5;


    CRect r(0, 0, iff, 18);
    if (modulate)
    {
        int hasMDIWS = infowindow->hasMDIWS();
        r.bottom += (hasMDIWS & modValues ? 36 : 18);
        if (modValues)
            r.right += 20;
    }

    CRect r2 = control->getControlViewSize();

    // OK this is a heuristic to stop deform overpainting and stuff
    if (r2.bottom > getWindowSizeY() - r.getHeight() - 2)
    {
        // stick myself on top please
        r.offset((r2.left / 150) * 150, r2.top - r.getHeight() - 2);
    }
    else
    {
        r.offset((r2.left / 150) * 150, r2.bottom);
    }

    if (r.bottom > getWindowSizeY() - 2)
    {
        int ao = (getWindowSizeY() - 2 - r.bottom);
        r.offset(0, ao);
    }

    if (r.right > getWindowSizeX() - 2)
    {
        int ao = (getWindowSizeX() - 2 - r.right);
        r.offset(ao, 0);
    }

    if (r.left < 0)
    {
        int ao = 2 - r.left;
        r.offset(ao, 0);
    }

    if (r.top < 0)
    {
        int ao = 2 - r.top;
        r.offset(0, ao);
    }

#endif
}
} // namespace Widgets
} // namespace Surge