#include "TuningOverlays.h"
#include "RuntimeFont.h"
#include "SurgeStorage.h"
#include "UserDefaults.h"
#include "fmt/core.h"
#include <chrono>

using namespace juce;

namespace Surge
{
namespace Overlays
{

class TuningTableListBoxModel : public juce::TableListBoxModel,
                                public Surge::GUI::SkinConsumingComponent
{
  public:
    TuningTableListBoxModel()
    {
        for (int i = 0; i < 128; ++i)
            notesOn[i] = false;

        rmbMenu = std::make_unique<juce::PopupMenu>();
    }
    ~TuningTableListBoxModel() { table = nullptr; }

    void setTableListBox(juce::TableListBox *t) { table = t; }

    void setupDefaultHeaders(juce::TableListBox *table)
    {
        table->setHeaderHeight(15);
        table->setRowHeight(13);
        table->getHeader().addColumn("Note", 1, 50);
        table->getHeader().addColumn("Freq (hz)", 2, 50);
    }

    virtual int getNumRows() override { return 128; }
    virtual void paintRowBackground(juce::Graphics &g, int rowNumber, int width, int height,
                                    bool rowIsSelected) override
    {
        if (!table)
            return;

        auto alternateColour =
            table->getLookAndFeel()
                .findColour(juce::ListBox::backgroundColourId)
                .interpolatedWith(table->getLookAndFeel().findColour(juce::ListBox::textColourId),
                                  0.03f);
        if (rowNumber % 2)
            g.fillAll(alternateColour);
    }

    int mcoff{1};
    void setMiddleCOff(int m)
    {
        mcoff = m;
        if (table)
            table->repaint();
    }
    virtual void paintCell(juce::Graphics &g, int rowNumber, int columnID, int width, int height,
                           bool rowIsSelected) override
    {
        if (!table)
            return;

        int noteInScale = rowNumber % 12;
        bool whitekey = true;
        bool noblack = false;
        if ((noteInScale == 1 || noteInScale == 3 || noteInScale == 6 || noteInScale == 8 ||
             noteInScale == 10))
        {
            whitekey = false;
        }
        if (noteInScale == 4 || noteInScale == 11)
            noblack = true;

        // Black Key
        auto kbdColour = table->getLookAndFeel().findColour(juce::ListBox::backgroundColourId);
        if (whitekey)
            kbdColour = kbdColour.interpolatedWith(
                table->getLookAndFeel().findColour(juce::ListBox::textColourId), 0.3f);

        bool no = true;
        auto pressedColour = juce::Colour(0xFFaaaa50);

        if (notesOn[rowNumber])
        {
            no = true;
            kbdColour = pressedColour;
        }

        g.fillAll(kbdColour);
        if (!whitekey && columnID != 1 && no)
        {
            g.setColour(table->getLookAndFeel().findColour(juce::ListBox::backgroundColourId));
            // draw an inset top and bottom
            g.fillRect(0, 0, width - 1, 1);
            g.fillRect(0, height - 1, width - 1, 1);
        }

        int txtOff = 0;
        if (columnID == 1)
        {
            // Black Key
            if (!whitekey)
            {
                txtOff = 10;
                // "Black Key"
                auto kbdColour =
                    table->getLookAndFeel().findColour(juce::ListBox::backgroundColourId);
                auto kbc = kbdColour.interpolatedWith(
                    table->getLookAndFeel().findColour(juce::ListBox::textColourId), 0.3f);
                g.setColour(kbc);
                g.fillRect(0, 0, txtOff, height);

                // OK so now check neighbors
                if (rowNumber > 0 && notesOn[rowNumber - 1])
                {
                    g.setColour(pressedColour);
                    g.fillRect(0, 0, txtOff, height / 2);
                }
                if (rowNumber < 127 && notesOn[rowNumber + 1])
                {
                    g.setColour(pressedColour);
                    g.fillRect(0, height / 2, txtOff, height / 2);
                }
                g.setColour(table->getLookAndFeel().findColour(juce::ListBox::backgroundColourId));
                g.fillRect(0, height / 2, txtOff, 1);

                if (no)
                {
                    g.fillRect(txtOff, 0, width - 1 - txtOff, 1);
                    g.fillRect(txtOff, height - 1, width - 1 - txtOff, 1);
                    g.fillRect(txtOff, 0, 1, height - 1);
                }
            }
        }

        g.setColour(table->getLookAndFeel().findColour(juce::ListBox::textColourId));

        auto mn = rowNumber;
        double fr = tuning.frequencyForMidiNote(mn);

        std::string notenum, notename, display;

        g.setColour(table->getLookAndFeel().findColour(juce::ListBox::backgroundColourId));
        g.fillRect(width - 1, 0, 1, height);
        if (noblack)
            g.fillRect(0, height - 1, width, 1);

        g.setColour(juce::Colours::white);
        auto just = juce::Justification::centredLeft;
        switch (columnID)
        {
        case 1:
        {
            notenum = std::to_string(mn);
            notename = noteInScale % 12 == 0 ? fmt::format("C{:d}", rowNumber / 12 - mcoff) : "";
            static std::vector<std::string> nn = {
                {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"}};

            g.setFont(Surge::GUI::getFontManager()->getLatoAtSize(9));
            g.drawText(notename, 2 + txtOff, 0, width - 4, height, juce::Justification::centredLeft,
                       false);
            g.setFont(Surge::GUI::getFontManager()->getLatoAtSize(7));
            g.drawText(notenum, 2 + txtOff, 0, width - 2 - txtOff - 2, height,
                       juce::Justification::centredRight, false);

            break;
        }
        case 2:
        {
            just = juce::Justification::centredRight;
            display = fmt::format("{:.2f}", fr);
            g.setFont(Surge::GUI::getFontManager()->getLatoAtSize(9));
            g.drawText(display, 2 + txtOff, 0, width - 4, height, juce::Justification::centredRight,
                       false);
            break;
        }
        }
    }

    virtual void cellClicked(int rowNumber, int columnId, const juce::MouseEvent &e) override
    {
        if (e.mods.isRightButtonDown())
        {
            rmbMenu->clear();
            rmbMenu->addItem("Export to CSV", [this]() { this->exportToCSV(); });
            rmbMenu->showMenuAsync(juce::PopupMenu::Options());
        }
    }

    std::unique_ptr<juce::FileChooser> fileChooser;
    virtual void exportToCSV()
    {
        fileChooser =
            std::make_unique<juce::FileChooser>("Export CSV to...", juce::File(), "*.csv");
        fileChooser->launchAsync(
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser &chooser) {
                auto f = chooser.getResult();
                std::ostringstream csvStream;
                csvStream << "Midi Note, Frequency, Log(Freq/8.17)\n";
                for (int i = 0; i < 128; ++i)
                    csvStream << i << ", " << std::fixed << std::setprecision(4)
                              << tuning.frequencyForMidiNote(i) << ", " << std::fixed
                              << std::setprecision(6) << tuning.logScaledFrequencyForMidiNote(i)
                              << "\n";
                if (!f.replaceWithText(csvStream.str()))
                {
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::AlertWindow::AlertIconType::WarningIcon, "Error exporting file",
                        "An unknown error occured streaming CSV data to file", "OK");
                }
            });
    }

    virtual void tuningUpdated(const Tunings::Tuning &newTuning)
    {
        tuning = newTuning;
        if (table)
            table->repaint();
    }
    virtual void noteOn(int noteNum)
    {
        notesOn[noteNum] = true;
        if (table)
            table->repaint();
    }
    virtual void noteOff(int noteNum)
    {
        notesOn[noteNum] = false;
        if (table)
            table->repaint();
    }

  private:
    Tunings::Tuning tuning;
    std::array<std::atomic<bool>, 128> notesOn;
    std::unique_ptr<juce::PopupMenu> rmbMenu;
    juce::TableListBox *table{nullptr};
};

class RadialScaleGraph;

class RadialScaleGraph : public juce::Component
{
  public:
    RadialScaleGraph() { setScale(Tunings::evenTemperament12NoteScale()); }

    void setScale(const Tunings::Scale &s)
    {
        scale = s;
        notesOn.clear();
        notesOn.resize(scale.count);
        for (int i = 0; i < scale.count; ++i)
            notesOn[i] = 0;
    }

    virtual void paint(juce::Graphics &g) override;
    Tunings::Scale scale;
    std::vector<juce::Rectangle<float>> screenHotSpots;
    int hotSpotIndex = -1;
    std::vector<int> notesOn;
    double dInterval, centsAtMouseDown, dIntervalAtMouseDown;

    juce::AffineTransform screenTransform, screenTransformInverted;
    std::function<void(int index, double)> onToneChanged = [](int, double) {};

    void noteOn(int sn)
    {
        if (sn < notesOn.size())
            notesOn[sn]++;
        repaint();
    }
    void noteOff(int sn)
    {
        if (sn < notesOn.size())
        {
            notesOn[sn]--;
            if (notesOn[sn] < 0)
                notesOn[sn] = 0;
        }
        repaint();
    }
    virtual void mouseMove(const juce::MouseEvent &e) override;
    virtual void mouseDown(const juce::MouseEvent &e) override;
    virtual void mouseDrag(const juce::MouseEvent &e) override;
};

void RadialScaleGraph::paint(Graphics &g)
{
    if (notesOn.size() != scale.count)
    {
        notesOn.clear();
        notesOn.resize(scale.count);
        for (int i = 0; i < scale.count; ++i)
            notesOn[i] = 0;
    }
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
    int w = getWidth();
    int h = getHeight();
    float r = std::min(w, h) / 2.1;
    float xo = (w - 2 * r) / 2.0;
    float yo = (h - 2 * r) / 2.0;
    double outerRadiusExtension = 0.4;

    g.saveState();

    screenTransform = AffineTransform::scale(1.0 / (1.0 + outerRadiusExtension * 1.1))
                          .scaled(r, -r)
                          .translated(r, r)
                          .translated(xo, yo);
    screenTransformInverted = screenTransform.inverted();

    g.addTransform(screenTransform);

    // We are now in a normal x y 0 1 coordinate system with 0,0 at the center. Cool

    // So first things first - scan for range.
    double ETInterval = scale.tones.back().cents / scale.count;
    double dIntMin = 0, dIntMax = 0;
    for (int i = 0; i < scale.count; ++i)
    {
        auto t = scale.tones[i];
        auto c = t.cents;

        auto intervalDistance = (c - ETInterval * (i + 1)) / ETInterval;
        dIntMax = std::max(intervalDistance, dIntMax);
        dIntMin = std::min(intervalDistance, dIntMin);
    }
    double range = std::max(0.01, std::max(dIntMax, -dIntMin / 2.0)); // twice as many inside rings
    int iRange = std::ceil(range);

    dInterval = outerRadiusExtension / iRange;
    double nup = iRange;
    double ndn = (int)(iRange * 1.6);

    // Now draw the interval circles
    for (int i = -ndn; i <= nup; ++i)
    {
        if (i == 0)
        {
        }
        else
        {
            float pos = 1.0 * std::abs(i) / ndn;
            float cpos = std::max(0.f, pos);

            g.setColour(Colour(110, 110, 120)
                            .interpolatedWith(
                                getLookAndFeel().findColour(ResizableWindow::backgroundColourId),
                                cpos * 0.8));

            float rad = 1.0 + dInterval * i;
            g.drawEllipse(-rad, -rad, 2 * rad, 2 * rad, 0.01);
        }
    }

    for (int i = 0; i < scale.count; ++i)
    {
        double frac = 1.0 * i / (scale.count);
        double sx = std::sin(frac * 2.0 * MathConstants<double>::pi);
        double cx = std::cos(frac * 2.0 * MathConstants<double>::pi);

        if (notesOn[i] > 0)
            g.setColour(Colour(255, 255, 255));
        else
            g.setColour(Colour(110, 110, 120));
        g.drawLine(0, 0, (1.0 + outerRadiusExtension) * sx, (1.0 + outerRadiusExtension) * cx,
                   0.01);

        g.saveState();
        g.addTransform(AffineTransform::rotation((-frac + 0.25) * 2.0 * MathConstants<double>::pi));
        g.addTransform(AffineTransform::translation(1.0 + outerRadiusExtension, 0.0));
        g.addTransform(AffineTransform::rotation(MathConstants<double>::pi * 0.5));
        g.addTransform(AffineTransform::scale(-1.0, 1.0));

        if (notesOn[i] > 0)
            g.setColour(Colour(255, 255, 255));
        else
            g.setColour(Colour(200, 200, 240));
        Rectangle<float> textPos(0, -0.1, 0.1, 0.1);
        g.setFont(0.1);
        g.drawText(juce::String(i), textPos, Justification::centred, 1);
        g.restoreState();
    }

    // Draw the ring at 1.0
    g.setColour(Colour(255, 255, 255));
    g.drawEllipse(-1, -1, 2, 2, 0.01);

    // Then draw ellipses for each note
    screenHotSpots.clear();

    for (int i = 1; i <= scale.count; ++i)
    {
        double frac = 1.0 * i / (scale.count);
        double sx = std::sin(frac * 2.0 * MathConstants<double>::pi);
        double cx = std::cos(frac * 2.0 * MathConstants<double>::pi);

        auto t = scale.tones[i - 1];
        auto c = t.cents;
        auto expectedC = scale.tones.back().cents / scale.count;

        auto rx = 1.0 + dInterval * (c - expectedC * i) / expectedC;

        float x0 = rx * sx - 0.05, y0 = rx * cx - 0.05, dx = 0.1, dy = 0.1;

        if (notesOn[i] > 0)
        {
            g.setColour(Colour(255, 255, 255));
            g.drawLine(sx, cx, rx * sx, rx * cx, 0.03);
        }

        Colour drawColour(200, 200, 200);

        // FIXME - this colormap is bad
        if (rx < 0.99)
        {
            // use a blue here
            drawColour = Colour(200 * (1.0 - 0.6 * rx), 200 * (1.0 - 0.6 * rx), 200);
        }
        else if (rx > 1.01)
        {
            // Use a yellow here
            drawColour = Colour(200, 200, 200 * (rx - 1.0));
        }

        if (hotSpotIndex == i - 1)
            drawColour = drawColour.brighter(0.6);

        g.setColour(drawColour);

        g.drawLine(sx, cx, rx * sx, rx * cx, 0.01);
        g.fillEllipse(x0, y0, dx, dy);

        if (hotSpotIndex != i - 1)
        {
            g.setColour(drawColour.brighter(0.6));
            g.drawEllipse(x0, y0, dx, dy, 0.01);
        }

        if (notesOn[i % scale.count] > 0)
        {
            g.setColour(Colour(255, 255, 255));
            g.drawEllipse(x0, y0, dx, dy, 0.02);
        }

        dx += x0;
        dy += y0;
        screenTransform.transformPoint(x0, y0);
        screenTransform.transformPoint(dx, dy);
        screenHotSpots.push_back(Rectangle<float>(x0, dy, dx - x0, y0 - dy));
    }

    g.restoreState();
}

void RadialScaleGraph::mouseMove(const juce::MouseEvent &e)
{
    int ohsi = hotSpotIndex;
    hotSpotIndex = -1;
    int h = 0;
    for (auto r : screenHotSpots)
    {
        if (r.contains(e.getPosition().toFloat()))
            hotSpotIndex = h;
        h++;
    }
    if (ohsi != hotSpotIndex)
        repaint();
}
void RadialScaleGraph::mouseDown(const juce::MouseEvent &e)
{
    if (hotSpotIndex == -1)
        centsAtMouseDown = 0;
    else
    {
        centsAtMouseDown = scale.tones[hotSpotIndex].cents;
        dIntervalAtMouseDown = dInterval;
    }
}

void RadialScaleGraph::mouseDrag(const juce::MouseEvent &e)
{
    if (hotSpotIndex != -1)
    {
        auto mdp = e.getMouseDownPosition().toFloat();
        auto xd = mdp.getX();
        auto yd = mdp.getY();
        screenTransformInverted.transformPoint(xd, yd);

        auto mp = e.getPosition().toFloat();
        auto x = mp.getX();
        auto y = mp.getY();
        screenTransformInverted.transformPoint(x, y);

        auto dr = -sqrt(xd * xd + yd * yd) + sqrt(x * x + y * y);
        dr = dr * 0.7; // FIXME - make this a variable
        onToneChanged(hotSpotIndex, centsAtMouseDown + 100 * dr / dIntervalAtMouseDown);
    }
}

struct SCLKBMDisplay : public juce::Component,
                       Surge::GUI::SkinConsumingComponent,
                       juce::TextEditor::Listener,
                       juce::Button::Listener
{
    SCLKBMDisplay()
    {
        scl = std::make_unique<juce::TextEditor>();
        scl->setFont(Surge::GUI::getFontManager()->getFiraMonoAtSize(10));
        scl->setMultiLine(true, false);
        scl->setReturnKeyStartsNewLine(true);
        scl->addListener(this);
        addAndMakeVisible(*scl);

        kbm = std::make_unique<juce::TextEditor>();
        kbm->setFont(Surge::GUI::getFontManager()->getFiraMonoAtSize(10));
        kbm->setMultiLine(true, false);
        kbm->setReturnKeyStartsNewLine(true);
        kbm->addListener(this);
        addAndMakeVisible(*kbm);

        apply = std::make_unique<juce::TextButton>("Apply", "Apply");
        apply->addListener(this);
        apply->setEnabled(false);
        addAndMakeVisible(*apply);
    }
    void setTuning(const Tunings::Tuning &t)
    {
        scl->setText(t.scale.rawText, juce::NotificationType::dontSendNotification);
        kbm->setText(t.keyboardMapping.rawText, juce::NotificationType::dontSendNotification);
        kbm->setText(t.keyboardMapping.rawText, juce::NotificationType::dontSendNotification);
        apply->setEnabled(false);
    }

    void resized() override
    {
        auto w = getWidth();
        auto h = getHeight() - 22;
        scl->setBounds(2, 2, w / 2 - 4, h - 4);
        kbm->setBounds(w / 2 + 2, 2, w / 2 - 4, h - 4);
        apply->setBounds(w - 102, h + 2, 100, 18);
    }

    void textEditorTextChanged(TextEditor &editor) override { apply->setEnabled(true); }
    void buttonClicked(Button *button) override
    {
        onTextChanged(scl->getText().toStdString(), kbm->getText().toStdString());
    }
    std::function<void(const std::string &scl, const std::string &kbl)> onTextChanged =
        [](auto a, auto b) {};

    std::unique_ptr<juce::TextEditor> scl, kbm;
    std::unique_ptr<juce::Button> apply;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SCLKBMDisplay);
};

TuningOverlay::TuningOverlay()
{
    tuning = Tunings::Tuning(Tunings::evenTemperament12NoteScale(),
                             Tunings::startScaleOnAndTuneNoteTo(60, 60, Tunings::MIDI_0_FREQ * 32));
    tuningKeyboardTableModel = std::make_unique<TuningTableListBoxModel>();
    tuningKeyboardTableModel->tuningUpdated(tuning);
    tuningKeyboardTable =
        std::make_unique<juce::TableListBox>("Tuning", tuningKeyboardTableModel.get());
    tuningKeyboardTableModel->setTableListBox(tuningKeyboardTable.get());
    tuningKeyboardTableModel->setupDefaultHeaders(tuningKeyboardTable.get());
    addAndMakeVisible(*tuningKeyboardTable);

    tuningKeyboardTable->getViewport()->setScrollBarsShown(true, false);
    tuningKeyboardTable->getViewport()->setViewPositionProportionately(0.0, 48.0 / 127.0);

    tabArea =
        std::make_unique<juce::TabbedComponent>(juce::TabbedButtonBar::Orientation::TabsAtBottom);

    sclKbmDisplay = std::make_unique<SCLKBMDisplay>();
    sclKbmDisplay->onTextChanged = [this](const std::string &s, const std::string &k) {
        this->onNewSCLKBM(s, k);
    };

    radialScaleGraph = std::make_unique<RadialScaleGraph>();
    radialScaleGraph->onToneChanged = [this](int note, double d) { this->onToneChanged(note, d); };

    tabArea->addTab("SCL/KBM", juce::Colours::black, sclKbmDisplay.get(), false);
    tabArea->addTab("Radial", juce::Colours::black, radialScaleGraph.get(), false);
    tabArea->addTab("Interval", juce::Colours::black, new juce::Label("S"), true);
    tabArea->addTab("Generators", juce::Colours::black, new juce::Label("S"), true);
    addAndMakeVisible(*tabArea);
}

TuningOverlay::~TuningOverlay() = default;

void TuningOverlay::resized()
{
    auto h = getHeight();
    auto w = getWidth();
    tuningKeyboardTable->setBounds(0, 0, 120, h);

    tabArea->setBounds(120, 0, w - 120, h);
    // it's a bit of a hack to put this here but by this [oint i'm all set up
    if (storage)
    {
        auto mcoff = Surge::Storage::getUserDefaultValue(storage, Surge::Storage::MiddleC, 1);
        tuningKeyboardTableModel->setMiddleCOff(mcoff);
    }
}

void TuningOverlay::onToneChanged(int tone, double newCentsValue)
{
    if (storage)
    {
        storage->currentScale.tones[tone].type = Tunings::Tone::kToneCents;
        storage->currentScale.tones[tone].cents = newCentsValue;
        recalculateScaleText();
    }
}

void TuningOverlay::onNewSCLKBM(const std::string &scl, const std::string &kbm)
{
    if (!storage)
        return;

    try
    {
        auto s = Tunings::parseSCLData(scl);
        auto k = Tunings::parseKBMData(kbm);
        storage->retuneAndRemapToScaleAndMapping(s, k);
        setTuning(storage->currentTuning);
    }
    catch (const Tunings::TuningError &e)
    {
        storage->reportError(e.what(), "Error Applying Tuning");
    }
}

void TuningOverlay::recalculateScaleText()
{
    std::ostringstream oss;
    oss << "! Scale generated by tuning editor\n"
        << storage->currentScale.description << "\n"
        << storage->currentScale.count << "\n"
        << "! \n";
    for (int i = 0; i < storage->currentScale.count; ++i)
    {
        auto tn = storage->currentScale.tones[i];
        if (tn.type == Tunings::Tone::kToneRatio)
        {
            oss << tn.ratio_n << "/" << tn.ratio_d << "\n";
        }
        else
        {
            oss << std::fixed << std::setprecision(5) << tn.cents << "\n";
        }
    }

    try
    {
        storage->retuneToScale(Tunings::parseSCLData(oss.str()));
        setTuning(storage->currentTuning);
    }
    catch (const Tunings::TuningError &e)
    {
    }
}

void TuningOverlay::setTuning(const Tunings::Tuning &t)
{
    tuning = t;
    tuningKeyboardTableModel->tuningUpdated(tuning);
    sclKbmDisplay->setTuning(t);
    radialScaleGraph->setScale(t.scale);
    repaint();
}

void TuningOverlay::onSkinChanged()
{
    tuningKeyboardTableModel->setSkin(skin, associatedBitmapStore);
    tuningKeyboardTable->repaint();
}

} // namespace Overlays
} // namespace Surge