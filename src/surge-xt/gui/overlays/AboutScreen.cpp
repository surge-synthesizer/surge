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
#include "SurgeImage.h"
#include "platform/Paths.h"
#include <fmt/core.h>

namespace Surge
{
namespace Overlays
{

struct ClickURLImage : public juce::Component
{
    ClickURLImage(SurgeImage *img, int offset, const std::string &URL)
        : url(URL), offset(offset), img(img)
    {
    }

    void paint(juce::Graphics &g) override
    {
        int ys = isHovered ? imgsz : 0;
        int xs = offset * imgsz;

        juce::Graphics::ScopedSaveState gs(g);

        auto t = juce::AffineTransform();
        t = t.translated(-xs, -ys);

        g.reduceClipRegion(0, 0, imgsz, imgsz);
        img->draw(g, 1.0, t);
    }

    void mouseEnter(const juce::MouseEvent &e) override
    {
        isHovered = true;
        repaint();
    }

    void mouseExit(const juce::MouseEvent &e) override
    {
        isHovered = false;
        repaint();
    }

    void mouseUp(const juce::MouseEvent &event) override
    {
        juce::URL(url).launchInDefaultBrowser();
    }

    bool isHovered{false};
    int offset, imgsz = 36;
    std::string url;
    SurgeImage *img;
};

struct HyperlinkLabel : public juce::Label, public Surge::GUI::SkinConsumingComponent
{
    HyperlinkLabel(const std::string &URL) : url(URL) {}

    void mouseEnter(const juce::MouseEvent &e) override
    {
        setColour(juce::Label::textColourId, skin->getColor(Colors::AboutPage::Link));
        repaint();
    }

    void mouseExit(const juce::MouseEvent &e) override
    {
        setColour(juce::Label::textColourId, skin->getColor(Colors::AboutPage::LinkHover));
        repaint();
    }

    void mouseUp(const juce::MouseEvent &event) override
    {
        juce::URL(url).launchInDefaultBrowser();
    }

    std::string url;
};

AboutScreen::AboutScreen() {}

AboutScreen::~AboutScreen() noexcept = default;

void AboutScreen::populateData()
{
    std::string version = std::string("Surge XT ") + Surge::Build::FullVersionStr;
    std::ostringstream oss;
    oss << Surge::Build::BuildDate << " @ " << Surge::Build::BuildTime << " on '"
        << Surge::Build::BuildHost << "/" << Surge::Build::BuildLocation << "' with '"
        << Surge::Build::BuildCompiler << "' using JUCE " << JUCE_MAJOR_VERSION << "."
        << JUCE_MINOR_VERSION << "." << JUCE_BUILDNUMBER;

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
    lowerLeft.emplace_back("Version:", version, "");
    lowerLeft.emplace_back("Build:", buildinfo, "");
    lowerLeft.emplace_back("System:", system, "");

    auto srString = fmt::format("{:.1f} kHz", samplerate / 1000.0);

    if (host != "Unknown")
    {
        auto hstr = host + " @ " + srString;
        lowerLeft.emplace_back("Host:", hstr, "");
    }
    else
    {
        lowerLeft.emplace_back("Sample Rate:", srString, "");
    }

    lowerLeft.emplace_back("", "", "");

    lowerLeft.emplace_back("Executable:", Paths::appPath().u8string(),
                           Paths::appPath().parent_path().u8string());
    lowerLeft.emplace_back("Factory Data:", storage->datapath.u8string(),
                           storage->datapath.u8string());
    lowerLeft.emplace_back("User Data:", storage->userDataPath.u8string(),
                           storage->userDataPath.u8string());

    lowerRight.clear();
    lowerRight.emplace_back("Current Skin:", skin->displayName, skin->root + skin->name);
    lowerRight.emplace_back("Skin Author:", skin->author, skin->authorURL);
}

void AboutScreen::buttonClicked(juce::Button *button)
{
    if (button == copyButton.get())
    {
        std::ostringstream oss;

        for (auto l : lowerLeft)
        {
            if (std::get<0>(l).empty())
            {
                break;
            }

            oss << std::get<0>(l) << " " << std::get<1>(l) << "\n";
        }

        juce::SystemClipboard::copyTextToClipboard(oss.str());
    }
}

void AboutScreen::resized()
{
    if (labels.empty())
    {
        int lHeight = 16;
        int margin = 16;
        auto rightSide = getWidth() - margin - 240;
        auto lls = lowerLeft.size();
        auto lrs = lowerRight.size();
        auto h0 = getHeight() - lls * lHeight - margin;
        auto h1 = getHeight() - lrs * lHeight - margin;
        auto colW = 66;
        auto font = Surge::GUI::getFontManager()->getLatoAtSize(10);

        copyButton = std::make_unique<juce::TextButton>();
        copyButton->setButtonText("Copy Version Info");
        copyButton->setBounds(margin + 4, h0 - lHeight - 10, 100, 20);
        copyButton->addListener(this);

        addAndMakeVisible(*copyButton);

        for (auto l : lowerLeft)
        {
            auto lb = std::make_unique<juce::Label>();
            lb->setInterceptsMouseClicks(false, true);
            lb->setText(std::get<0>(l), juce::NotificationType::dontSendNotification);
            lb->setBounds(margin, h0, colW, lHeight);
            lb->setFont(font);
            lb->setColour(juce::Label::textColourId, skin->getColor(Colors::AboutPage::ColumnText));
            addAndMakeVisible(*lb);
            labels.push_back(std::move(lb));

            if (std::get<2>(l).empty())
            {
                auto lb = std::make_unique<juce::Label>();
                lb->setInterceptsMouseClicks(false, true);
                lb->setText(std::get<1>(l), juce::NotificationType::dontSendNotification);
                lb->setBounds(margin + colW, h0, getWidth() - margin - colW, lHeight);
                lb->setFont(font);
                lb->setColour(juce::Label::textColourId, skin->getColor(Colors::AboutPage::Text));
                addAndMakeVisible(*lb);
                labels.push_back(std::move(lb));
            }
            else
            {
                auto lb = std::make_unique<HyperlinkLabel>(std::get<2>(l));
                lb->setText(std::get<1>(l), juce::NotificationType::dontSendNotification);
                lb->setBounds(margin + colW, h0, getWidth() - margin - colW, lHeight);
                lb->setFont(font);
                lb->setSkin(skin, associatedBitmapStore);
                lb->setColour(juce::Label::textColourId, skin->getColor(Colors::AboutPage::Link));
                addAndMakeVisible(*lb);
                labels.push_back(std::move(lb));
            }

            h0 += lHeight;
        }

        for (auto l : lowerRight)
        {
            auto lb = std::make_unique<juce::Label>();

            lb->setInterceptsMouseClicks(false, true);
            lb->setText(std::get<0>(l), juce::NotificationType::dontSendNotification);
            lb->setBounds(rightSide, h1, colW, lHeight);
            lb->setFont(font);
            lb->setColour(juce::Label::textColourId, skin->getColor(Colors::AboutPage::ColumnText));
            addAndMakeVisible(*lb);
            labels.push_back(std::move(lb));

            if (std::get<2>(l).empty())
            {
                auto lb = std::make_unique<juce::Label>();
                lb->setInterceptsMouseClicks(false, true);
                lb->setText(std::get<1>(l), juce::NotificationType::dontSendNotification);
                lb->setBounds(rightSide + colW, h1, getWidth() - margin - colW, lHeight);
                lb->setFont(font);
                lb->setColour(juce::Label::textColourId, skin->getColor(Colors::AboutPage::Text));
                addAndMakeVisible(*lb);
                labels.push_back(std::move(lb));
            }
            else
            {
                auto lb = std::make_unique<HyperlinkLabel>(std::get<2>(l));
                lb->setText(std::get<1>(l), juce::NotificationType::dontSendNotification);
                lb->setBounds(rightSide + colW, h1, getWidth() - margin - colW, lHeight);
                lb->setFont(font);
                lb->setSkin(skin, associatedBitmapStore);
                lb->setColour(juce::Label::textColourId, skin->getColor(Colors::AboutPage::Link));
                addAndMakeVisible(*lb);
                labels.push_back(std::move(lb));
            }

            h1 += lHeight;
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

        auto img = associatedBitmapStore->getImage(IDB_ABOUT_LOGOS);
        auto idxes = {0, 4, 3, 1, 2, 5};

        std::vector<std::string> urls = {
            "https://github.com/surge-synthesizer/surge/",
            "https://discord.gg/aFQDdMV",
            "https://www.gnu.org/licenses/gpl-3.0-standalone.html",
            "https://www.steinberg.net/en/company/technologies/vst3.html",
            "https://developer.apple.com/documentation/audiounit",
            "https://juce.com"};

        int x = 0;

        for (auto idx : idxes)
        {
            auto bt = std::make_unique<ClickURLImage>(img, idx, urls[idx]);

            bt->setBounds(rightSide + (x * 40), margin, 36, 36);
            addAndMakeVisible(*bt);
            icons.push_back(std::move(bt));

            x++;
        }
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
    {
        editor->hideAboutScreen();
    }
}

void AboutScreen::onSkinChanged()
{
    logo = associatedBitmapStore->getImage(IDB_ABOUT_BG);

    for (const auto &l : labels)
    {
        auto skc = dynamic_cast<Surge::GUI::SkinConsumingComponent *>(l.get());

        if (skc)
        {
            skc->setSkin(skin, associatedBitmapStore);
        }
    }
}

} // namespace Overlays
} // namespace Surge
