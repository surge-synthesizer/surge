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

#include "AboutScreen.h"
#include "SurgeGUIEditor.h"
#include "SurgeStorage.h"
#include "CPUFeatures.h"
#include "version.h"
#include "RuntimeFont.h"

namespace Surge
{
namespace Overlays
{
struct NoUrlHyperlinkButton : public juce::HyperlinkButton
{
    NoUrlHyperlinkButton() : HyperlinkButton("", juce::URL()) {}
    void clicked() override {}
};

AboutScreen::AboutScreen() {}

AboutScreen::~AboutScreen() noexcept = default;

void AboutScreen::populateData()
{
    std::string version = std::string("Surge XT ") + Surge::Build::FullVersionStr;
    std::ostringstream oss;
    oss << Surge::Build::BuildDate << " @ " << Surge::Build::BuildTime << " on '"
        << Surge::Build::BuildHost << "/" << Surge::Build::BuildLocation << "' with '"
        << Surge::Build::BuildCompiler << "' using JUCE " << std::hex << JUCE_VERSION << std::dec;

    std::string buildinfo = oss.str();

#if MAC
    std::string platform = "macOS";
#elif WINDOWS
    std::string platform = "Windows";
#elif LINUX
    std::string platform = "Linux";
#else
    std::string platform = "GLaDOS, Orac or Skynet";
#endif

    std::string bitness = (sizeof(size_t) == 4 ? std::string("32") : std::string("64")) + "-bit";
    std::string system =
        platform + " " + bitness + " " + wrapper + " on " + Surge::CPUFeatures::cpuBrand();

    lowerLeft.clear();
    lowerLeft.emplace_back("Version", version, "");
    lowerLeft.emplace_back("Build", buildinfo, "");
    lowerLeft.emplace_back("System", system, "");
    if (host != "Unknown")
        lowerLeft.emplace_back("Host", host, "");
    lowerLeft.emplace_back("", "", "");
    lowerLeft.emplace_back("Factory Data", storage->datapath, storage->datapath);
    lowerLeft.emplace_back("User Data", storage->userDataPath, storage->userDataPath);
    lowerLeft.emplace_back("Plugin", storage->installedPath, "");
    lowerLeft.emplace_back("Current Skin", skin->displayName, skin->root + skin->name);
    lowerLeft.emplace_back("Skin Author", skin->author, skin->authorURL);

    for (auto &l : lowerLeft)
    {
        std::cout << std::get<0>(l) << "  -   " << std::get<1>(l) << std::endl;
    }
}

void AboutScreen::buttonClicked(juce::Button *button)
{
    if (button == copyButton.get())
    {
        std::ostringstream oss;
        for (auto l : lowerLeft)
        {
            if (std::get<0>(l).empty())
                break;
            oss << std::get<0>(l) << ":  " << std::get<1>(l) << "\n";
        }
        juce::SystemClipboard::copyTextToClipboard(oss.str());
    }
}

void AboutScreen::resized()
{
    if (labels.empty())
    {
        std::cout << "Building on resize " << getWidth() << " " << getHeight() << std::endl;

        int lHeight = 16;
        int margin = 5;
        auto lls = lowerLeft.size();
        auto h0 = getHeight() - lls * lHeight - margin;
        auto colW = 66;

        copyButton = std::make_unique<juce::TextButton>();
        copyButton->setButtonText("Copy to Clipboard");
        copyButton->setBounds(margin, h0 - 20 - margin, 100, 20);
        copyButton->addListener(this);
        addAndMakeVisible(*copyButton);

        for (auto l : lowerLeft)
        {
            auto lb = std::make_unique<juce::Label>();
            lb->setInterceptsMouseClicks(false, true);
            lb->setText(std::get<0>(l), juce::NotificationType::dontSendNotification);
            lb->setBounds(margin, h0, colW, lHeight);
            lb->setFont(Surge::GUI::getFontManager()->getLatoAtSize(10));
            lb->setColour(juce::Label::textColourId, skin->getColor(Colors::AboutPage::ColumnText));
            addAndMakeVisible(*lb);
            labels.push_back(std::move(lb));

            lb = std::make_unique<juce::Label>();
            lb->setInterceptsMouseClicks(false, true);
            lb->setText(std::get<1>(l), juce::NotificationType::dontSendNotification);
            lb->setBounds(margin + colW, h0, getWidth() - margin - colW, lHeight);
            lb->setFont(Surge::GUI::getFontManager()->getLatoAtSize(10));
            lb->setColour(juce::Label::textColourId, skin->getColor(Colors::AboutPage::Text));
            addAndMakeVisible(*lb);
            labels.push_back(std::move(lb));

            h0 += lHeight;
        }

        auto xp = margin;
        auto yp = margin;
        auto lblvs = lHeight * .8;

        auto addLabel = [this, &xp, &yp, lHeight](const std::string &s, int w) {
            auto lb = std::make_unique<juce::Label>();
            lb->setInterceptsMouseClicks(false, true);
            lb->setText(s, juce::NotificationType::dontSendNotification);
            lb->setBounds(xp, yp, w, lHeight);
            lb->setFont(Surge::GUI::getFontManager()->getLatoAtSize(8));
            lb->setColour(juce::Label::textColourId, skin->getColor(Colors::AboutPage::Text));
            addAndMakeVisible(*lb);
            labels.push_back(std::move(lb));
        };

        addLabel(
            std::string("Copyright 2005-") + Surge::Build::BuildYear +
                " by Vember Audio and individual contributors in the Surge Synth Team, released "
                "under the GNU GPL v3 license",
            600);
        yp += lblvs;
        addLabel("Built using the JUCE framework.", 600);
        yp += lblvs;
        addLabel("VST is a trademark of Steinberg Media Technologies GmbH;Audio Units is a "
                 "trademark of Apple Inc.",
                 600);
        yp += lblvs;
        addLabel("Airwindows open source effects by Chris Johnson, licensed under MIT license",
                 600);
        yp += lblvs;
        addLabel("OB-Xd filters by Vadim Filatov, licensed under GNU GPL v3 license", 600);
        yp += lblvs;
        addLabel("K35 and Diode Ladder filters by Will Pirkle (implementation by TheWaveWarden), "
                 "licensed under GNU GPL v3 license",
                 600);
        yp += lblvs;
        addLabel("Cutoff Warp and Resonance Warp filters; CHOW, Neuron and Tape effects by Jatin "
                 "Chowdhury, licensed under GNU GPL v3 license",
                 600);
        yp += lblvs;
        addLabel(
            "Exciter effect and BBD delay line emulation by Jatin Chowdhury, licensed under BSD "
            "3-clause license",
            600);
        yp += lblvs;
        addLabel("OJD waveshaper by Janos Buttgereit, licensed under GNU GPL v3 license", 600);
        yp += lblvs;
        addLabel(
            "Nimbus effect and Twist oscillator based on firmware for Eurorack hardware modules "
            "by Émilie Gillet, licensed under MIT license",
            600);

        auto lb = std::make_unique<juce::Label>();
        lb->setInterceptsMouseClicks(false, false);
        lb->setText("Github / Discord / JUCE logo", juce::NotificationType::dontSendNotification);
        lb->setColour(juce::Label::textColourId, juce::Colours::orchid);
        lb->setBounds(500, margin, 300, 50);
        addAndMakeVisible(*lb);
        labels.push_back(std::move(lb));
#if 0
        auto iconsize = CPoint(36, 36);
    auto ranchor = vs.right - iconsize.x - margin;
    auto spacing = 56;

    yp = margin;

    // place these right to left respecting right anchor (ranchor) and icon spacing
    auto discord = new CSurgeHyperlink(CRect(CPoint(ranchor - (spacing * 0), yp), iconsize));
    discord->setURL("https://discord.gg/aFQDdMV");
    discord->setBitmap(bitmapStore->getBitmap(IDB_ABOUT_LOGOS));
    discord->setHorizOffset(CCoord(144));
    addView(discord);

    auto gplv3 = new CSurgeHyperlink(CRect(CPoint(ranchor - (spacing * 1), yp), iconsize));
    gplv3->setURL("https://www.gnu.org/licenses/gpl-3.0-standalone.html");
    gplv3->setBitmap(bitmapStore->getBitmap(IDB_ABOUT_LOGOS));
    gplv3->setHorizOffset(CCoord(108));
    addView(gplv3);

    auto au = new CSurgeHyperlink(CRect(CPoint(ranchor - (spacing * 2), yp), iconsize));
    au->setURL("https://developer.apple.com/documentation/audiounit");
    au->setBitmap(bitmapStore->getBitmap(IDB_ABOUT_LOGOS));
    au->setHorizOffset(CCoord(72));
    addView(au);

    auto vst3 = new CSurgeHyperlink(CRect(CPoint(ranchor - (spacing * 3), yp), iconsize));
    vst3->setURL("https://www.steinberg.net/en/company/technologies/vst3.html");
    vst3->setBitmap(bitmapStore->getBitmap(IDB_ABOUT_LOGOS));
    vst3->setHorizOffset(CCoord(36));
    addView(vst3);

    auto gh = new CSurgeHyperlink(CRect(CPoint(ranchor - (spacing * 4), yp), iconsize));
    gh->setURL("https://github.com/surge-synthesizer/surge/");
    gh->setBitmap(bitmapStore->getBitmap(IDB_ABOUT_LOGOS));
    gh->setHorizOffset(CCoord(0));
    addView(gh);
#endif
    }
}

void AboutScreen::paint(juce::Graphics &g)
{
    g.fillAll(fillColour);

    if (logo)
    {
        auto r = juce::Rectangle<int>(0, 0, logoW, logoH).withCentre(getLocalBounds().getCentre());
        logo->drawWithin(g, r.toFloat(), juce::RectanglePlacement(), 1.0);
    }
}

void AboutScreen::mouseUp(const juce::MouseEvent &e)
{
    if (editor)
        editor->hideAboutScreen();
}

void AboutScreen::onSkinChanged() { logo = associatedBitmapStore->getDrawable(IDB_ABOUT_BG); }

} // namespace Overlays
} // namespace Surge
