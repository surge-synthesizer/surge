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

#include "AboutScreen.h"
#include "SurgeGUIEditor.h"
#include "SurgeStorage.h"
#include "version.h"
#include "RuntimeFont.h"
#include "SurgeImage.h"
#include "sst/plugininfra/paths.h"
#include "sst/plugininfra/cpufeatures.h"
#include <fmt/core.h>

namespace Surge
{
namespace Overlays
{

struct ClickURLImage : public juce::Component
{
    ClickURLImage(SurgeImage *img, int offset, const std::string &URL, const std::string &title)
        : url(URL), offset(offset), img(img)
    {
        setAccessible(true);
        setTitle(title);
        setDescription(title);
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

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
    {
        return std::make_unique<juce::AccessibilityHandler>(
            *this, juce::AccessibilityRole::button,
            juce::AccessibilityActions().addAction(juce::AccessibilityActionType::press, [this]() {
                juce::URL(url).launchInDefaultBrowser();
            }));
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
        setColour(juce::Label::textColourId, skin->getColor(Colors::AboutPage::LinkHover));
        repaint();
    }

    void mouseExit(const juce::MouseEvent &e) override
    {
        setColour(juce::Label::textColourId, skin->getColor(Colors::AboutPage::Link));
        repaint();
    }

    void mouseUp(const juce::MouseEvent &event) override
    {
        juce::URL(url).launchInDefaultBrowser();
    }

    std::string url;
};

struct ClipboardCopyButton : public juce::TextButton, Surge::GUI::SkinConsumingComponent
{
    ClipboardCopyButton() : juce::TextButton()
    {
        setAccessible(true);
        setWantsKeyboardFocus(true);
        setDescription("Copy Info to Clipboard");
        setTitle("Copy Info to Clipboard");
    }

    void paintButton(juce::Graphics &g, bool shouldDrawButtonAsHighlighted,
                     bool shouldDrawButtonAsDown) override
    {
        assert(skin.get());

        if (isOver())
        {
            g.setColour(skin->getColor(Colors::AboutPage::LinkHover));
        }
        else
        {
            g.setColour(skin->getColor(Colors::AboutPage::Link));
        }

        g.setFont(skin->fontManager->getLatoAtSize(10));
        g.drawText("Copy Info to Clipboard", getLocalBounds(), juce::Justification::centredLeft,
                   false);
    }
};

AboutScreen::AboutScreen()
{
    setAccessible(true);
    setFocusContainerType(juce::Component::FocusContainerType::keyboardFocusContainer);
    setWantsKeyboardFocus(true);
    std::string version = std::string("About Surge XT ") + Surge::Build::FullVersionStr;
    setTitle(version);
    setDescription(version);
}

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
#if defined(_M_ARM64EC)
    platform += " (arm64ec)";
#elif defined(_M_ARM64)
    platform += " (arm64)";
#endif
#elif LINUX
    std::string platform = "Linux";
#else
    std::string platform = "GLaDOS, Orac or Skynet";
#endif

    const auto ramsize = juce::SystemStats::getMemorySizeInMegabytes();
    const auto ramString =
        fmt::format("{:.0f} {} RAM", ramsize >= 1024 ? std::roundf(ramsize / 1024.f) : ramsize,
                    ramsize >= 1024 ? "GB" : "MB");

    const auto bitness = (sizeof(size_t) == 4 ? std::string("32") : std::string("64")) + "-bit";
    const auto system = fmt::format("{} {}{} on {}, {}", platform, bitness,
                                    wrapper == "Undefined" ? "" : " " + wrapper,
                                    sst::plugininfra::cpufeatures::brand(), ramString);

    lowerLeft.clear();
    lowerLeft.emplace_back("Version:", version, "");
    lowerLeft.emplace_back("Build Info:", buildinfo, "");
    lowerLeft.emplace_back("System Info:", system, "");

    const auto srString = fmt::format("{} Hz", (int)storage->samplerate);

    if (host != "Unknown")
    {
        auto hstr = host + " @ " + srString;

        lowerLeft.emplace_back("Host:", hstr, "");
    }
    else
    {
        lowerLeft.emplace_back("Sample Rate:", srString, "");
    }

    lowerLeft.emplace_back("Processing Block:", fmt::format("{} samples", BLOCK_SIZE), "");

    lowerLeft.emplace_back("", "", "");

    const auto apppath = sst::plugininfra::paths::sharedLibraryBinaryPath();

    lowerLeft.emplace_back("Executable:", apppath.u8string(), apppath.parent_path().u8string());
    lowerLeft.emplace_back("Factory Data:", storage->datapath.u8string(),
                           storage->datapath.u8string());
    lowerLeft.emplace_back("User Data:", storage->userDataPath.u8string(),
                           storage->userDataPath.u8string());

    lowerRight.clear();

    auto skinFullName = skin->displayName;

    if (!skin->category.empty())
    {
        skinFullName = skin->category + " - " + skin->displayName;
    }

    lowerRight.emplace_back("Current Skin:", skinFullName, skin->root + skin->name);
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
    if (labels.empty() && devModeGrid == -1)
    {
        int lHeight = 16;
        int margin = 16;
        auto rightSide = getWidth() - margin - 252;
        auto lls = lowerLeft.size();
        auto lrs = lowerRight.size();
        auto h0 = getHeight() - lls * lHeight - margin;
        auto h1 = getHeight() - lrs * lHeight - margin;
        auto colW = 84;
        auto font = skin->fontManager->getLatoAtSize(10);

        copyButton = std::make_unique<ClipboardCopyButton>();
        copyButton->setSkin(skin, associatedBitmapStore);
        copyButton->setBounds(margin + 4, h0 - lHeight - 4, 112, 16);
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
                lb->setFont(font);
                lb->setColour(juce::Label::textColourId, skin->getColor(Colors::AboutPage::Text));
                lb->setText(std::get<1>(l), juce::NotificationType::dontSendNotification);
                lb->setBounds(margin + colW, h0, getWidth() - margin - colW, lHeight);

                addAndMakeVisible(*lb);
                labels.push_back(std::move(lb));
            }
            else
            {
                auto lb = std::make_unique<HyperlinkLabel>(std::get<2>(l));
                lb->setSkin(skin, associatedBitmapStore);
                lb->setFont(font);
                lb->setColour(juce::Label::textColourId, skin->getColor(Colors::AboutPage::Link));
                lb->setText(std::get<1>(l), juce::NotificationType::dontSendNotification);

                auto strw = SST_STRING_WIDTH_INT(font, std::get<1>(l)) + 8;
                lb->setBounds(margin + colW, h0, strw, lHeight);

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
                lb->setFont(font);
                lb->setColour(juce::Label::textColourId, skin->getColor(Colors::AboutPage::Text));
                lb->setText(std::get<1>(l), juce::NotificationType::dontSendNotification);
                lb->setBounds(rightSide + colW, h1, getWidth() - margin - colW, lHeight);

                addAndMakeVisible(*lb);
                labels.push_back(std::move(lb));
            }
            else
            {
                auto lb = std::make_unique<HyperlinkLabel>(std::get<2>(l));
                lb->setSkin(skin, associatedBitmapStore);
                lb->setFont(font);
                lb->setColour(juce::Label::textColourId, skin->getColor(Colors::AboutPage::Link));
                lb->setText(std::get<1>(l), juce::NotificationType::dontSendNotification);

                auto strw = SST_STRING_WIDTH_INT(font, std::get<1>(l)) + 8;
                lb->setBounds(rightSide + colW, h1, strw, lHeight);

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
            lb->setFont(skin->fontManager->getLatoAtSize(8));
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

        addLabel("VST is a trademark of Steinberg Media Technologies GmbH; Audio Units is a "
                 "trademark of Apple Inc.",
                 600);

        yp += lblvs;

        addLabel("CLAP support is licensed under MIT license", 600);

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

        addLabel("Cutoff Warp, Resonance Warp and Tri-Pole filters; CHOW, Neuron and Tape effects "
                 "by Jatin Chowdhury, licensed under GNU GPL v3 license",
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

        yp += lblvs;

#if EURORACK_CLOUDS_IS_SUPERPARASITES
        addLabel("Nimbus includes the the integrated superparasites firmware by Patrick Dowling, "
                 "including"
                 " mqtthiqs/Parasites and jkammerl/Beat Repeat modes.",
                 600);

        yp += lblvs;
#endif

        addLabel("Oscilloscope code based on s(m)exoscope by Bram @ Smartelectronix, licensed "
                 "under GNU GPL v3 license",
                 600);

        auto img = associatedBitmapStore->getImage(IDB_ABOUT_LOGOS);
        auto idxes = {0, 4, 3, 6, 1, 2, 5};

        std::vector<std::string> urls = {
            stringRepository,
            "https://www.steinberg.net/en/company/technologies/vst3.html",
            "https://developer.apple.com/documentation/audiounit",
            "https://www.gnu.org/licenses/gpl-3.0-standalone.html",
            "https://discord.gg/aFQDdMV",
            "https://juce.com",
            "https://cleveraudio.org"};

        std::vector<std::string> urllabels = {
            "Surge XT GitHub Repository", "Steinberg VST3", "Apple Audio Units",  "GNU GPL3",
            "Join our Discord!",          "JUCE Framework", "CLever Audio Plugin"};

        int x = idxes.size();

        for (auto idx : idxes)
        {
            auto bt = std::make_unique<ClickURLImage>(img, idx, urls[idx], urllabels[idx]);

            bt->setBounds(getWidth() - (margin / 2) - (x * 42), margin, 36, 36);
            addAndMakeVisible(*bt);
            icons.push_back(std::move(bt));

            x--;
        }
    }
}

void AboutScreen::paint(juce::Graphics &g)
{
    if (devModeGrid == -1)
    {
        g.fillAll(fillColour);

        if (logo)
        {
            auto r =
                juce::Rectangle<int>(0, 0, logoW, logoH).withCentre(getLocalBounds().getCentre());
            logo->drawWithin(g, r.toFloat(), juce::RectanglePlacement(), 1.0);
        }
    }
    else
    {
        auto primaryLineColor = juce::Colour((uint32_t)0xC0FF2020);
        auto secondaryLineColor = juce::Colour((uint32_t)0xC0C06060);

        g.setFont(skin->fontManager->getLatoAtSize(9));
        g.setColour(juce::Colours::red);
        g.drawText("0", juce::Rectangle(2, 2, 20, 40), juce::Justification::topLeft);

        // x axis
        for (int i = 1; i <= getHeight() / devModeGrid; i++)
        {
            int y = i * devModeGrid;

            if (i % 4 == 0)
            {
                g.setColour(juce::Colours::red);
                g.drawText(std::to_string(y), juce::Rectangle(2, y + 2, 20, 40),
                           juce::Justification::topLeft);

                g.setColour(primaryLineColor);
            }
            else
            {
                g.setColour(secondaryLineColor);
            }

            g.drawLine(0, y, getWidth(), y);
        }

        // y axis
        for (int i = 1; i <= getWidth() / devModeGrid; i++)
        {
            int x = i * devModeGrid;

            if (i % 4 == 0)
            {
                g.setColour(juce::Colours::red);
                g.drawText(std::to_string(x), juce::Rectangle(x + 2, 2, 40, 20),
                           juce::Justification::topLeft);

                g.setColour(primaryLineColor);
            }
            else
            {
                g.setColour(secondaryLineColor);
            }

            g.drawLine(x, 0, x, getHeight());
        }
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

    auto skcb = dynamic_cast<Surge::GUI::SkinConsumingComponent *>(copyButton.get());

    if (skcb)
    {
        skcb->setSkin(skin, associatedBitmapStore);
    }

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
