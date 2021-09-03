#include "TuningOverlays.h"

using namespace juce;

namespace Surge
{
namespace Overlays
{

class InfiniteKnob : public Component
{
  public:
    InfiniteKnob() : Component(), angle(0) {}

    virtual void mouseDown(const MouseEvent &event) override
    {
        lastDrag = 0;
        isDragging = true;
        repaint();
    }
    virtual void mouseDrag(const MouseEvent &event) override
    {
        int d = -(event.getDistanceFromDragStartX() + event.getDistanceFromDragStartY());
        int diff = d - lastDrag;
        lastDrag = d;
        if (diff != 0)
        {
            float mul = 1.0;
            if (event.mods.isShiftDown())
            {
                mul = 0.05;
            }
            angle += diff * mul;
            onDragDelta(diff * mul);
            repaint();
        }
    }
    virtual void mouseUp(const MouseEvent &event) override
    {
        isDragging = false;
        repaint();
    }
    virtual void paint(Graphics &g) override
    {
        int w = getWidth();
        int h = getHeight();
        int b = std::min(w, h);
        float r = b / 2.0;
        float dx = (w - b) / 2.0;
        float dy = (h - b) / 2.0;
        g.saveState();
        g.addTransform(AffineTransform::translation(dx, dy));
        g.addTransform(AffineTransform::translation(r, r));
        g.addTransform(AffineTransform::rotation(angle / 50.0 * 2.0 * MathConstants<double>::pi));
        g.setColour(getLookAndFeel().findColour(juce::Slider::rotarySliderFillColourId));
        g.fillEllipse(-(r - 3), -(r - 3), (r - 3) * 2, (r - 3) * 2);
        g.setColour(getLookAndFeel().findColour(juce::GroupComponent::outlineColourId));
        g.drawEllipse(-(r - 3), -(r - 3), (r - 3) * 2, (r - 3) * 2, r / 5.0);
        if (enabled)
        {
            g.setColour(getLookAndFeel().findColour(juce::Slider::thumbColourId));
            g.drawLine(0, -(r - 1), 0, r - 1, r / 3.0);
        }
        g.restoreState();
    }

    int lastDrag = 0;
    bool isDragging = false;
    float angle;
    std::function<void(float)> onDragDelta = [](float f) {};
    bool enabled = true;
};

class ScaleEditor::RadialScaleGraph : public juce::Component
{
  public:
    RadialScaleGraph(Tunings::Scale &s) : scale(s)
    {
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

class ScaleEditor::GeneratorSection : public juce::Component,
                                      public juce::Button::Listener,
                                      public juce::TextEditor::Listener
{
  public:
    GeneratorSection(ScaleEditor *ed) : editor(ed)
    {

        int xpos = 0;
        {
            evenDivLabel.reset(new Label("edl", "Even Division of M into N Tones"));
            addAndMakeVisible(evenDivLabel.get());
            evenDivLabel->setBounds(xpos, 0, 210, 22);
            evenDivLabel->setJustificationType(Justification::centred);

            int lxp = xpos;
            evenDivMLabel.reset(new Label("edm", "M"));
            addAndMakeVisible(evenDivMLabel.get());
            evenDivMLabel->setBounds(lxp, 29, 20, 22);
            lxp += 20 + 4;

            evenDivM.reset(new TextEditor("divm"));
            addAndMakeVisible(evenDivM.get());
            evenDivM->setBounds(lxp, 29, 56, 22);
            evenDivM->setText("2", dontSendNotification);
            lxp += 56 + 6;

            evenDivNLabel.reset(new Label("edm", "N"));
            addAndMakeVisible(evenDivNLabel.get());
            evenDivNLabel->setBounds(lxp, 29, 20, 22);
            lxp += 20 + 4;

            evenDivN.reset(new TextEditor("divn"));
            addAndMakeVisible(evenDivN.get());
            evenDivN->setBounds(lxp, 29, 56, 22);
            evenDivN->setText("12", dontSendNotification);
            lxp += 56 + 4;

            evenDivApply.reset(new TextButton("ap"));
            addAndMakeVisible(evenDivApply.get());
            evenDivApply->setBounds(lxp, 29, 46, 22);
            evenDivApply->setButtonText("Apply");
            evenDivApply->addListener(this);
        }

        xpos += 210 + 4;

        xpos += 10;
        Path p;
        p.addRectangle(xpos, 0, 1, 29 + 22);
        p1.reset(new DrawablePath());
        addAndMakeVisible(p1.get());
        p1->setBounds(xpos, 0, 215, 29 + 22);
        p1->setFill(getLookAndFeel().findColour(GroupComponent::outlineColourId));
        p1->setPath(p);
        xpos += 10;

        {
            setOctaveLabel.reset(new Label("so", "Set Period To"));
            addAndMakeVisible(setOctaveLabel.get());
            setOctaveLabel->setBounds(xpos, 0, 100, 22);

            setOctaveTE.reset(new TextEditor("sote"));
            addAndMakeVisible(setOctaveTE.get());
            setOctaveTE->setText(String(editor->scale.tones.back().cents, 5), dontSendNotification);
            setOctaveTE->setBounds(xpos, 29, 100, 22);
            setOctaveTE->addListener(this);
            xpos += 108;

            setOctaveKnob.reset(new InfiniteKnob());
            addAndMakeVisible(setOctaveKnob.get());
            setOctaveKnob->setBounds(xpos, 0, 29 + 22, 29 + 22);
            setOctaveKnob->onDragDelta = [this](float f) {
                auto tb = this->editor->scale.tones.back().cents;
                if (f > 0)
                {
                    tb *= (1.0 + 0.005 * f);
                }
                else
                {
                    tb /= (1.0 - 0.005 * f);
                }
                this->setOctaveTE->setText(String(tb, 5));
            };
            xpos += 29 + 22 + 4;
        }

        xpos += 10;
        Path pp;
        pp.addRectangle(xpos, 0, 1, 29 + 22);
        p2.reset(new DrawablePath());
        addAndMakeVisible(p2.get());
        p2->setBounds(xpos, 0, 215, 29 + 22);
        p2->setFill(getLookAndFeel().findColour(GroupComponent::outlineColourId));
        p2->setPath(pp);
        xpos += 10;

        {
            compress1.reset(new Label("so", "Adjust towards or"));
            addAndMakeVisible(compress1.get());
            compress1->setBounds(xpos, 0, 140, 22);
            compress2.reset(new Label("so", "away from Even Temp"));
            addAndMakeVisible(compress2.get());
            compress2->setBounds(xpos, 22, 140, 22);

            compress1->setColour(Label::textColourId, Colour(190, 190, 200));
            compress2->setColour(Label::textColourId, Colour(190, 190, 200));

            xpos += 148;

            compressKnob.reset(new InfiniteKnob());
            addAndMakeVisible(compressKnob.get());
            compressKnob->setBounds(xpos, 0, 29 + 22, 29 + 22);
            compressKnob->enabled = false;
            xpos += 29 + 22 + 4;
        }

        xpos += 10;
        Path ppp;
        ppp.addRectangle(xpos, 0, 1, 29 + 22);
        p3.reset(new DrawablePath());
        addAndMakeVisible(p3.get());
        p3->setBounds(xpos, 0, 215, 29 + 22);
        p3->setFill(getLookAndFeel().findColour(GroupComponent::outlineColourId));
        p3->setPath(ppp);
        xpos += 10;

        {
            resetL.reset(new Label("so", "Reset to 12-TET"));
            addAndMakeVisible(resetL.get());
            resetL->setBounds(xpos, 0, 100, 22);

            resetB.reset(new TextButton("rb"));
            addAndMakeVisible(resetB.get());
            resetB->setButtonText("Reset");
            resetB->setBounds(xpos, 29, 100, 22);
            resetB->addListener(this);
            xpos += 108;
        }
    }

    virtual void buttonClicked(Button *buttonThatWasClicked) override
    {
        if (buttonThatWasClicked == evenDivApply.get())
        {
            try
            {
                auto s = Tunings::evenDivisionOfSpanByM(evenDivM->getText().getIntValue(),
                                                        evenDivN->getText().getIntValue());
                newScale(s);
            }
            catch (Tunings::TuningError &e)
            {
                AlertWindow::showMessageBoxAsync(AlertWindow::AlertIconType::WarningIcon,
                                                 "Error generating scale", e.what(), "OK");
            }
        }

        if (buttonThatWasClicked == resetB.get())
        {
            try
            {
                auto s = Tunings::evenTemperament12NoteScale();
                newScale(s);
                setOctaveTE->setText("1200.00000");
            }
            catch (Tunings::TuningError &e)
            {
                AlertWindow::showMessageBoxAsync(AlertWindow::AlertIconType::WarningIcon,
                                                 "Error resetting scale", e.what(), "OK");
            }
        }
    }

    virtual void textEditorTextChanged(TextEditor &e) override
    {
        if (&e == setOctaveTE.get())
        {
            auto f = std::atof(e.getText().toStdString().c_str());
            auto ratio = f / editor->scale.tones.back().cents;
            auto news = editor->scale;
            int i = 0;
            auto tones = editor->scale.tones;
            for (auto &t : tones)
            {
                auto c = t.cents * ratio;
                try
                {
                    editor->scale.tones[i] = Tunings::toneFromString(String(c, 5).toStdString());
                }
                catch (Tunings::TuningError &e)
                {
                    // Oh wells
                }
                i++;
            }
            editor->recalculateScaleText();
            newScale(editor->scale);
        }
    }

    void newScale(Tunings::Scale &s)
    {
        for (auto sl : editor->listeners)
            sl->scaleTextEdited(s.rawText);

        editor->radialScaleGraph->scale = s;
        editor->radialScaleGraph->repaint();
    }

    ScaleEditor *editor;
    std::unique_ptr<Label> evenDivLabel, evenDivMLabel, evenDivNLabel;
    std::unique_ptr<TextEditor> evenDivM, evenDivN;
    std::unique_ptr<Button> evenDivApply;

    std::unique_ptr<Label> setOctaveLabel;
    std::unique_ptr<TextEditor> setOctaveTE;
    std::unique_ptr<InfiniteKnob> setOctaveKnob;

    std::unique_ptr<Label> compress1, compress2;
    std::unique_ptr<InfiniteKnob> compressKnob;

    std::unique_ptr<Label> resetL;
    std::unique_ptr<Button> resetB;

    std::unique_ptr<DrawablePath> p1, p2, p3;
};

class NoteLED : public Component, public juce::AsyncUpdater
{
  public:
    void setNotes(int n)
    {
        notes = n;
        triggerAsyncUpdate();
    }

    virtual void handleAsyncUpdate() override { repaint(); }

    virtual void paint(Graphics &g) override
    {
        g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

        if (notes > 0)
        {
            g.setColour(Colour(200, 200, 215));
            g.fillEllipse(1, 1, 8, 8);
            g.setColour(Colour(120, 120, 255));
            g.fillEllipse(2, 2, 6, 6);
        }
        else
        {
            g.setColour(Colour(10, 10, 20));
            g.fillEllipse(2, 2, 6, 6);
        }
    }
    int notes;
};

ScaleEditor::ToneEditor::ToneEditor(bool editable)
{
    int xpos = 2;
    playingLED.reset(new NoteLED());
    addAndMakeVisible(playingLED.get());
    playingLED->setBounds(xpos, 7, 10, 10);
    xpos += 12;

    displayIndex.reset(new Label("idx"));
    addAndMakeVisible(displayIndex.get());
    displayIndex->setBounds(xpos, 2, 30, 20);
    xpos += 34;

    displayValue.reset(new TextEditor("display value"));
    addAndMakeVisible(displayValue.get());
    displayValue->setBounds(xpos, 2, 100, 20);
    displayValue->addListener(this);
    displayValue->setReadOnly(!editable);
    xpos += 104;

    if (editable)
    {
        auto ck = new InfiniteKnob();
        ck->onDragDelta = [this](float f) {
            auto nv = this->cents + f;
            displayValue->setText(String(nv, 5));
        };
        addAndMakeVisible(ck);
        coarseKnob.reset(ck);
        coarseKnob->setBounds(xpos, 2, 20, 20);
        xpos += 24;

        auto fk = new InfiniteKnob();
        addAndMakeVisible(fk);
        fk->onDragDelta = [this](float f) {
            auto nv = this->cents + f * 0.05 * 0.1;
            displayValue->setText(String(nv, 5));
        };

        fineKnob.reset(fk);
        fineKnob->setBounds(xpos, 2, 20, 20);
        xpos += 24;
    }
    else
    {
        hideButton.reset(new TextButton("Hide Values"));
        hideButton->setButtonText("Hide");
        hideButton->setBounds(xpos, 0, 45, 22);
        hideButton->addListener(this);
        addAndMakeVisible(hideButton.get());
    }
    setSize(xpos, 24);
}

void ScaleEditor::ToneEditor::buttonClicked(juce::Button *b) { parent->toggleToneDisplay(); }

void ScaleEditor::ToneEditor::displayText(bool dt)
{
    if (dt)
    {
        displayValue->setPasswordCharacter(0);
        if (hideButton)
            hideButton->setButtonText("Hide");
    }
    else
    {
        displayValue->setPasswordCharacter(0x2022);
        if (hideButton)
            hideButton->setButtonText("Show");
    }
}

void ScaleEditor::ToneEditor::incNotes()
{
    playingNotes++;
    if (playingLED.get())
    {
        dynamic_cast<NoteLED *>(playingLED.get())->setNotes(playingNotes);
    }
}

void ScaleEditor::ToneEditor::decNotes()
{
    playingNotes--;
    if (playingLED.get())
    {
        dynamic_cast<NoteLED *>(playingLED.get())->setNotes(playingNotes);
    }
}

ScaleEditor::ScaleEditor(Tunings::Scale &inScale)
{
    scale = inScale;
    scaleText = scale.rawText;

    buildUIFromScale();

    setSize(750, 500);
}

ScaleEditor::~ScaleEditor() {}

void ScaleEditor::buildUIFromScale()
{
    if (notesSection == nullptr)
    {
        int w = 750, h = 500;
        int tonesW = 250;
        int genHeight = 100;
        int m = 4;

        // It's my first time through<
        notesGroup.reset(new juce::GroupComponent("st", TRANS("Scale Tones")));
        addAndMakeVisible(notesGroup.get());
        notesGroup->setBounds(m, m, tonesW - 2 * m, h - genHeight - 2 * m);

        generatorGroup.reset(new juce::GroupComponent("cdg", TRANS("Scale Generators")));
        addAndMakeVisible(generatorGroup.get());
        generatorGroup->setBounds(m, h - genHeight - m, w, genHeight);

        generatorSection.reset(new GeneratorSection(this));
        addAndMakeVisible(generatorSection.get());
        generatorSection->setBounds(generatorGroup->getBounds().withTrimmedTop(15).reduced(m));

        notesSection = std::make_unique<Component>();
        notesViewport = std::make_unique<Viewport>();
        notesViewport->setViewedComponent(notesSection.get(), false);
        notesViewport->setBounds(notesGroup->getBounds().withTrimmedTop(15).reduced(m));
        notesViewport->setScrollBarsShown(true, false);
        addAndMakeVisible(notesViewport.get());

        analyticsGroup.reset(new juce::GroupComponent("st", TRANS("Analytics")));
        addAndMakeVisible(analyticsGroup.get());
        analyticsGroup->setBounds(tonesW + m, m, w - tonesW - m, h - genHeight - 2 * m);

        analyticsTab.reset(new juce::TabbedComponent(TabbedButtonBar::TabsAtTop));
        addAndMakeVisible(analyticsTab.get());
        analyticsTab->setBounds(analyticsGroup->getBounds().withTrimmedTop(15).reduced(m));
        analyticsTab->setTabBarDepth(30);
        radialScaleGraph = new RadialScaleGraph(scale);
        radialScaleGraph->onToneChanged = [this](int idx, double val) {
            if (idx >= 0 && idx < this->scale.count)
            {
                this->scale.tones[idx].type = Tunings::Tone::kToneCents;
                this->scale.tones[idx].cents = val;
                this->recalculateScaleText();
            }
        };

        analyticsTab->addTab(TRANS("Graph"),
                             getLookAndFeel().findColour(ResizableWindow::backgroundColourId),
                             radialScaleGraph, true);
        analyticsTab->addTab(TRANS("Matrix"),
                             getLookAndFeel().findColour(ResizableWindow::backgroundColourId),
                             new Label("cs", "Coming Soon"), true);
        analyticsTab->addTab(TRANS("Intervals"),
                             getLookAndFeel().findColour(ResizableWindow::backgroundColourId),
                             new Label("cs", "Coming Soon"), true);
    }

    radialScaleGraph->scale = scale;

    // clear prior components. Only need to do this if size has changed though. FIXME
    int nEds = scale.count;
    if (toneEditors.size() != scale.count + 1)
    {
        for (auto &p : toneEditors)
            notesSection->removeChildComponent(p.get());
        toneEditors.clear();
        for (int i = 0; i < nEds + 1; ++i)
        {
            auto t = std::make_unique<ToneEditor>(i != 0);
            notesSection->addAndMakeVisible(t.get());
            t->setBounds(10, 28 * i, 200, 24);
            t->displayText(displayTones);
            t->parent = this;
            toneEditors.push_back(std::move(t));
        }
    }

    for (int i = 0; i < nEds + 1; ++i)
    {
        auto t = toneEditors[i].get();
        if (i == 0)
        {
            t->displayValue->setText("1", dontSendNotification); // make it all readonly too
            t->displayIndex->setText("0", dontSendNotification);
            t->index = 0;
            t->cents = 0;
        }
        else
        {
            auto tn = scale.tones[i - 1];
            if (tn.type == Tunings::Tone::kToneRatio)
            {
                t->displayValue->setText(
                    juce::String(std::to_string(tn.ratio_n) + "/" + std::to_string(tn.ratio_d)),
                    dontSendNotification);
            }
            else
            {
                t->displayValue->setText(juce::String(std::to_string(tn.cents)),
                                         dontSendNotification);
            }
            t->displayIndex->setText(juce::String(std::to_string(i)), dontSendNotification);
            t->index = i;
            t->cents = tn.cents;
            t->onToneChanged = [this](int idx, juce::String newVal) {
                try
                {
                    this->scale.tones[idx - 1] = Tunings::toneFromString(newVal.toStdString());
                    this->toneEditors[idx]->cents = this->scale.tones[idx - 1].cents;
                    this->recalculateScaleText();
                }
                catch (Tunings::TuningError &e)
                {
                    // Set background to red
                    std::cout << "Tuning Error " << e.what() << std::endl;
                }
            };
        }
    }
    notesSection->setSize(380, 30 * (nEds + 1));

    repaint();
}

void ScaleEditor::toggleToneDisplay()
{
    displayTones = !displayTones;
    for (auto &c : toneEditors)
        c->displayText(displayTones);
}

void ScaleEditor::RadialScaleGraph::paint(Graphics &g)
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

void ScaleEditor::recalculateScaleText()
{
    std::ostringstream oss;
    oss << "! Scale generated by tuning editor\n"
        << scale.description << "\n"
        << scale.count << "\n"
        << "! \n";
    for (int i = 0; i < scale.count; ++i)
    {
        auto tn = scale.tones[i];
        if (tn.type == Tunings::Tone::kToneRatio)
        {
            oss << tn.ratio_n << "/" << tn.ratio_d << "\n";
        }
        else
        {
            oss << std::fixed << std::setprecision(5) << tn.cents << "\n";
        }
    }

    juce::String ns(
        oss.str().c_str()); // sort of a dumb set of copies here. FIXME references and stuff
    for (auto sl : listeners)
        sl->scaleTextEdited(ns);

    radialScaleGraph->scale = scale;
    radialScaleGraph->repaint();
}

void ScaleEditor::RadialScaleGraph::mouseMove(const juce::MouseEvent &e)
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
void ScaleEditor::RadialScaleGraph::mouseDown(const juce::MouseEvent &e)
{
    if (hotSpotIndex == -1)
        centsAtMouseDown = 0;
    else
    {
        centsAtMouseDown = scale.tones[hotSpotIndex].cents;
        dIntervalAtMouseDown = dInterval;
    }
}

void ScaleEditor::RadialScaleGraph::mouseDrag(const juce::MouseEvent &e)
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

void ScaleEditor::scaleNoteOn(int scaleNote)
{
    if (scaleNote < toneEditors.size())
        toneEditors[scaleNote]->incNotes();
    if (scaleNote == 0)
        toneEditors.back()->incNotes();
    radialScaleGraph->noteOn(scaleNote);
}
void ScaleEditor::scaleNoteOff(int scaleNote)
{
    if (scaleNote < toneEditors.size())
        toneEditors[scaleNote]->decNotes();
    if (scaleNote == 0)
        toneEditors.back()->decNotes();
    radialScaleGraph->noteOff(scaleNote);
}

} // namespace Overlays
} // namespace Surge