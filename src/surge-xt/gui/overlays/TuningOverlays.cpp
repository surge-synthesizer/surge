#include "TuningOverlays.h"
#include "RuntimeFont.h"
#include "SurgeStorage.h"
#include "UserDefaults.h"
#include "SurgeGUIUtils.h"
#include "SurgeGUIEditor.h"
#include "SurgeSynthEditor.h" // bit gross but so I can do DnD
#include "widgets/MultiSwitch.h"
#include "fmt/core.h"
#include <chrono>
#include "juce_gui_extra/juce_gui_extra.h"

namespace Surge
{
namespace Overlays
{

class TuningTableListBoxModel : public juce::TableListBoxModel,
                                public Surge::GUI::SkinConsumingComponent
{
  public:
    TuningTableListBoxModel() {}
    ~TuningTableListBoxModel() { table = nullptr; }

    void setTableListBox(juce::TableListBox *t) { table = t; }

    void setupDefaultHeaders(juce::TableListBox *table)
    {
        table->setHeaderHeight(12);
        table->setRowHeight(12);
        table->getHeader().addColumn("Note", 1, 30, 30, 30,
                                     juce::TableHeaderComponent::ColumnPropertyFlags::visible);
        table->getHeader().addColumn("Frequency (Hz)", 2, 50, 50, 50,
                                     juce::TableHeaderComponent::ColumnPropertyFlags::visible);
        table->getHeader().setPopupMenuActive(false);
    }

    virtual int getNumRows() override { return 128; }
    virtual void paintRowBackground(juce::Graphics &g, int rowNumber, int width, int height,
                                    bool rowIsSelected) override
    {
        if (!table)
            return;

        g.fillAll(skin->getColor(Colors::TuningOverlay::FrequencyKeyboard::Separator));
    }

    int mcoff{1};

    void setMiddleCOff(int m)
    {
        mcoff = m;

        if (table)
        {
            table->repaint();
        }
    }

    virtual void paintCell(juce::Graphics &g, int rowNumber, int columnID, int width, int height,
                           bool rowIsSelected) override
    {
        namespace clr = Colors::TuningOverlay::FrequencyKeyboard;

        if (!table)
        {
            return;
        }

        int noteInScale = rowNumber % 12;
        bool whitekey = true;
        bool noblack = false;

        if ((noteInScale == 1 || noteInScale == 3 || noteInScale == 6 || noteInScale == 8 ||
             noteInScale == 10))
        {
            whitekey = false;
        }

        if (noteInScale == 4 || noteInScale == 11)
        {
            noblack = true;
        }

        // black key
        auto kbdColour = skin->getColor(clr::BlackKey);

        if (whitekey)
        {
            kbdColour = skin->getColor(clr::WhiteKey);
        }

        bool no = false;
        auto pressedColour = skin->getColor(clr::PressedKey);

        if (notesOn[rowNumber])
        {
            no = true;
            kbdColour = pressedColour;
        }

        g.fillAll(kbdColour);

        if (!whitekey && columnID != 1)
        {
            g.setColour(skin->getColor(clr::Separator));
            // draw an inset top and bottom
            g.fillRect(0, 0, width - 1, 1);
            g.fillRect(0, height - 1, width - 1, 1);
        }

        int txtOff = 0;

        if (columnID == 1)
        {
            // black key
            if (!whitekey)
            {
                txtOff = 10;
                // "black key"
                auto kbdColour = skin->getColor(clr::BlackKey);
                auto kbc = skin->getColor(clr::WhiteKey);

                g.setColour(kbc);
                g.fillRect(-1, 0, txtOff, height + 2);

                // OK so now check neighbors
                if (rowNumber > 0 && notesOn[rowNumber - 1])
                {
                    g.setColour(pressedColour);
                    g.fillRect(0, 0, txtOff, height / 2);
                }

                if (rowNumber < 127 && notesOn[rowNumber + 1])
                {
                    g.setColour(pressedColour);
                    g.fillRect(0, height / 2, txtOff, height / 2 + 1);
                }

                g.setColour(skin->getColor(clr::BlackKey));
                g.fillRect(0, height / 2, txtOff, 1);

                if (no)
                {
                    g.fillRect(txtOff, 0, width - 1 - txtOff, 1);
                    g.fillRect(txtOff, height - 1, width - 1 - txtOff, 1);
                    g.fillRect(txtOff, 0, 1, height - 1);
                }
            }
        }

        auto mn = rowNumber;
        double fr = tuning.frequencyForMidiNote(mn);

        std::string notenum, notename, display;

        g.setColour(skin->getColor(clr::Separator));
        g.fillRect(width - 1, 0, 1, height);

        if (noblack)
        {
            g.fillRect(0, height - 1, width, 1);
        }

        g.setColour(skin->getColor(clr::Text));

        if (no)
        {
            g.setColour(skin->getColor(clr::PressedKeyText));
        }

        int margin = 5;
        auto just_l = juce::Justification::centredLeft;
        auto just_r = juce::Justification::centredRight;

        switch (columnID)
        {
        case 1:
        {
            notenum = std::to_string(mn);
            notename = noteInScale % 12 == 0 ? fmt::format("C{:d}", rowNumber / 12 - mcoff) : "";

            g.setFont(skin->fontManager->getLatoAtSize(7, juce::Font::bold));
            g.drawText(notename, 2 + txtOff, 0, width - margin, height, just_l, false);
            g.setFont(skin->fontManager->getLatoAtSize(7));
            g.drawText(notenum, 2 + txtOff, 0, width - txtOff - margin, height,
                       juce::Justification::centredRight, false);

            break;
        }
        case 2:
        {
            display = fmt::format("{:.2f}", fr);
            g.setFont(skin->fontManager->getLatoAtSize(8));
            g.drawText(display, 2 + txtOff, 0, width - margin, height, just_r, false);
            break;
        }
        }
    }

    virtual void cellClicked(int rowNumber, int columnId, const juce::MouseEvent &e) override {}

    virtual void tuningUpdated(const Tunings::Tuning &newTuning)
    {
        tuning = newTuning;
        if (table)
            table->repaint();
    }

    Tunings::Tuning tuning;
    std::bitset<128> notesOn;
    std::unique_ptr<juce::PopupMenu> rmbMenu;
    juce::TableListBox *table{nullptr};
};

class RadialScaleGraph;

class InfiniteKnob : public juce::Component, public Surge::GUI::SkinConsumingComponent
{
  public:
    InfiniteKnob() : juce::Component(), angle(0) {}

    virtual void mouseDown(const juce::MouseEvent &event) override
    {
        if (!storage || !Surge::GUI::showCursor(storage))
        {
            juce::Desktop::getInstance().getMainMouseSource().enableUnboundedMouseMovement(true);
        }

        lastDrag = 0;
        isDragging = true;
        repaint();
    }

    virtual void mouseDrag(const juce::MouseEvent &event) override
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

    void mouseWheelMove(const juce::MouseEvent &event,
                        const juce::MouseWheelDetails &wheel) override
    {
        /*
         * If I choose based on horiz/vert it only works on trackpads, so just add
         */
        float delta = wheel.deltaX - (wheel.isReversed ? 1 : -1) * wheel.deltaY;
        if (delta == 0)
            return;

#if MAC
        float speed = 1.2;
#else
        float speed = 0.42666;
#endif
        if (event.mods.isShiftDown())
        {
            speed = speed / 10;
        }

        // This is calibrated to give us reasonable speed on a 0-1 basis from the slider
        // but in this widget '1' is the small motion so speed it up some
        speed *= 30;

        angle += delta * speed;
        onDragDelta(delta * speed);
        repaint();
    }

    virtual void mouseUp(const juce::MouseEvent &e) override
    {
        if (!storage || !Surge::GUI::showCursor(storage))
        {
            juce::Desktop::getInstance().getMainMouseSource().enableUnboundedMouseMovement(false);
            auto p = localPointToGlobal(e.mouseDownPosition);
            juce::Desktop::getInstance().getMainMouseSource().setScreenPosition(p);
        }

        isDragging = false;
        repaint();
    }

    virtual void mouseDoubleClick(const juce::MouseEvent &e) override
    {
        onDoubleClick();
        angle = 0;
        repaint();
    }

    virtual void paint(juce::Graphics &g) override
    {
        if (!skin)
            return;

        namespace clr = Colors::TuningOverlay::RadialGraph;

        int w = getWidth();
        int h = getHeight();
        int b = std::min(w, h);
        float r = b / 2.0;
        float dx = (w - b) / 2.0;
        float dy = (h - b) / 2.0;

        g.saveState();
        g.addTransform(juce::AffineTransform::translation(dx, dy));
        g.addTransform(juce::AffineTransform::translation(r, r));
        g.addTransform(
            juce::AffineTransform::rotation(angle / 50.0 * 2.0 * juce::MathConstants<double>::pi));

        if (isHovered)
        {
            g.setColour(skin->getColor(clr::KnobFillHover));
        }
        else
        {
            g.setColour(skin->getColor(clr::KnobFill));
        }

        g.fillEllipse(-(r - 3), -(r - 3), (r - 3) * 2, (r - 3) * 2);
        g.setColour(skin->getColor(clr::KnobBorder));
        g.drawEllipse(-(r - 3), -(r - 3), (r - 3) * 2, (r - 3) * 2, r / 5.0);

        if (enabled)
        {
            if (isPlaying)
            {
                g.setColour(skin->getColor(clr::KnobThumbPlayed));
            }
            else
            {
                g.setColour(skin->getColor(clr::KnobThumb));
            }

            g.drawLine(0, -(r - 1), 0, r - 1, r / 3.0);
        }

        g.restoreState();
    }

    bool isHovered = false;

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

    int lastDrag = 0;
    bool isDragging = false;
    bool isPlaying{false};
    float angle;
    std::function<void(float)> onDragDelta = [](float f) {};
    std::function<void()> onDoubleClick = []() {};
    bool enabled = true;
    SurgeStorage *storage{nullptr};
};

class RadialScaleGraph : public juce::Component,
                         public juce::TextEditor::Listener,
                         public Surge::GUI::SkinConsumingComponent,
                         public Surge::GUI::IComponentTagValue::Listener
{
  public:
    RadialScaleGraph()
    {
        toneList = std::make_unique<juce::Viewport>();
        toneInterior = std::make_unique<juce::Component>();
        toneList->setViewedComponent(toneInterior.get(), false);
        addAndMakeVisible(*toneList);

        setTuning(Tunings::Tuning(Tunings::evenTemperament12NoteScale(), Tunings::tuneA69To(440)));
    }

    SurgeStorage *storage{nullptr};

    void setTuning(const Tunings::Tuning &t)
    {
        int priorLen = tuning.scale.count;
        tuning = t;
        scale = t.scale;

        if (toneEditors.empty() || priorLen != scale.count || toneEditors.size() != scale.count)
        {
            toneInterior->removeAllChildren();
            auto w = usedForSidebar - 10;
            auto m = 4;
            auto h = 20;
            auto labw = 18;

            toneInterior->setSize(w, (scale.count + 1) * (h + m));
            toneEditors.clear();
            toneKnobs.clear();
            toneLabels.clear();

            for (int i = 0; i < scale.count + 1; ++i)
            {
                auto totalR = juce::Rectangle<int>(m, i * (h + m) + m, w - 2 * m, h);

                auto tl = std::make_unique<juce::Label>("tone index");
                if (skin)
                {
                    tl->setFont(skin->fontManager->getLatoAtSize(9));
                }
                tl->setText(std::to_string(i), juce::NotificationType::dontSendNotification);
                tl->setBounds(totalR.withWidth(labw));
                tl->setJustificationType(juce::Justification::centredRight);
                toneInterior->addAndMakeVisible(*tl);
                toneLabels.push_back(std::move(tl));

                if (i == 0)
                {
                    auto tk = std::make_unique<InfiniteKnob>();
                    tk->storage = storage;

                    tk->setBounds(totalR.withX(totalR.getWidth() - h).withWidth(h).reduced(2));
                    tk->onDragDelta = [this](float f) {
                        selfEditGuard++;
                        onScaleRescaled(f);
                        selfEditGuard--;
                    };
                    tk->onDoubleClick = [this]() {
                        selfEditGuard++;
                        onScaleRescaledAbsolute(1200.0);
                        selfEditGuard--;
                    };
                    if (skin)
                        tk->setSkin(skin, associatedBitmapStore);
                    toneInterior->addAndMakeVisible(*tk);
                    toneKnobs.push_back(std::move(tk));

                    showHideKnob = std::make_unique<Surge::Widgets::MultiSwitchSelfDraw>();
                    showHideKnob->setSkin(skin, associatedBitmapStore);
                    showHideKnob->setStorage(storage);
                    showHideKnob->setRows(1);
                    showHideKnob->setColumns(1);
                    showHideKnob->setTag(12345);
                    showHideKnob->setLabels({"Hide"});
                    showHideKnob->addListener(this);

                    showHideKnob->setBounds(
                        totalR.withTrimmedLeft(labw + m).withTrimmedRight(h + m).reduced(3));

                    toneInterior->addAndMakeVisible(*showHideKnob);
                }
                else
                {
                    auto te = std::make_unique<juce::TextEditor>("tone");
                    te->setBounds(totalR.withTrimmedLeft(labw + m).withTrimmedRight(h + m));
                    te->setJustification((juce::Justification::verticallyCentred));
                    if (skin)
                    {
                        te->setFont(skin->fontManager->getFiraMonoAtSize(9));
                    }
                    te->setIndents(4, (te->getHeight() - te->getTextHeight()) / 2);
                    te->setText(std::to_string(i), juce::NotificationType::dontSendNotification);
                    te->setEnabled(i != 0);
                    te->addListener(this);

                    te->onEscapeKey = [this]() { giveAwayKeyboardFocus(); };
                    toneInterior->addAndMakeVisible(*te);
                    toneEditors.push_back(std::move(te));

                    auto tk = std::make_unique<InfiniteKnob>();

                    tk->setBounds(totalR.withX(totalR.getWidth() - h).withWidth(h).reduced(2));
                    tk->onDragDelta = [this, i](float f) {
                        int idx = i;
                        if (idx == 0)
                            idx = scale.count;
                        auto ct = scale.tones[idx - 1].cents;
                        ct += f;
                        selfEditGuard++;
                        onToneChanged(idx - 1, ct);
                        selfEditGuard--;
                    };

                    tk->onDoubleClick = [this, i]() {
                        auto ri = scale.tones[scale.count - 1].cents;
                        auto nc = ri / scale.count * i;
                        selfEditGuard++;
                        onToneChanged(i - 1, nc);
                        selfEditGuard--;
                    };

                    if (skin)
                        tk->setSkin(skin, associatedBitmapStore);
                    toneInterior->addAndMakeVisible(*tk);
                    toneKnobs.push_back(std::move(tk));
                }
            }
        }

        for (int i = 0; i < scale.count; ++i)
        {
            auto td = fmt::format("{:.5f}", scale.tones[i].cents);
            if (scale.tones[i].type == Tunings::Tone::kToneRatio)
            {
                td = fmt::format("{:d}/{:d}", scale.tones[i].ratio_n, scale.tones[i].ratio_d);
            }
            toneEditors[i]->setText(td, juce::NotificationType::dontSendNotification);
        }

        notesOn.clear();
        notesOn.resize(scale.count);
        for (int i = 0; i < scale.count; ++i)
            notesOn[i] = false;
        setNotesOn(bitset);

        if (selfEditGuard == 0)
        {
            // Someone dragged something onto me or something. Reset the tuning knobs
            for (const auto &tk : toneKnobs)
            {
                tk->angle = 0;
                tk->repaint();
            }
        }
    }

    bool centsShowing{true};

    void valueChanged(GUI::IComponentTagValue *p) override
    {
        if (p == showHideKnob.get())
        {
            centsShowing = !centsShowing;
            for (const auto &t : toneEditors)
            {
                t->setPasswordCharacter(centsShowing ? 0 : 0x2022);
            }
            showHideKnob->setLabels({centsShowing ? "Hide" : "Show"});
        }
        repaint();
    }

    void onSkinChanged() override
    {
        if (showHideKnob)
        {
            showHideKnob->setSkin(skin, associatedBitmapStore);
        }

        for (const auto &k : toneKnobs)
        {
            k->setSkin(skin, associatedBitmapStore);
        }

        for (const auto &tl : toneLabels)
        {
            tl->setFont(skin->fontManager->getLatoAtSize(9));
        }

        for (const auto &te : toneEditors)
        {
            te->setFont(skin->fontManager->getFiraMonoAtSize(9));
            // Work around a juce feature that replacing a font only works on new text
            auto gt = te->getText();
            te->setText("val", juce::dontSendNotification);
            te->setText(gt, juce::dontSendNotification);
        }

        setNotesOn(bitset);
        repaint();
    }

  private:
    void textEditorReturnKeyPressed(juce::TextEditor &editor) override;

  public:
    virtual void paint(juce::Graphics &g) override;
    Tunings::Tuning tuning;
    Tunings::Scale scale;
    std::vector<juce::Rectangle<float>> screenHotSpots;
    int hotSpotIndex = -1, wheelHotSpotIndex = -1;
    double dInterval, centsAtMouseDown, angleAtMouseDown, dIntervalAtMouseDown;

    int selfEditGuard{0};

    juce::AffineTransform screenTransform, screenTransformInverted;
    std::function<void(int index, double)> onToneChanged = [](int, double) {};
    std::function<void(int index, const std::string &s)> onToneStringChanged =
        [](int, const std::string &) {};
    std::function<void(double)> onScaleRescaled = [](double) {};
    std::function<void(double)> onScaleRescaledAbsolute = [](double) {};
    static constexpr int usedForSidebar = 160;

    std::unique_ptr<juce::Viewport> toneList;
    std::unique_ptr<juce::Component> toneInterior;
    std::vector<std::unique_ptr<juce::TextEditor>> toneEditors;
    std::vector<std::unique_ptr<juce::Label>> toneLabels;
    std::vector<std::unique_ptr<InfiniteKnob>> toneKnobs;
    std::unique_ptr<Surge::Widgets::MultiSwitchSelfDraw> showHideKnob;

    void resized() override { toneList->setBounds(0, 0, usedForSidebar, getHeight()); }

    std::vector<bool> notesOn;
    std::bitset<128> bitset{0};
    void setNotesOn(const std::bitset<128> &bs)
    {
        namespace clr = Colors::TuningOverlay::RadialGraph;
        bitset = bs;

        for (int i = 0; i < scale.count; ++i)
        {
            notesOn[i] = false;
        }

        for (int i = 0; i < 128; ++i)
        {
            if (bitset[i])
            {
                notesOn[tuning.scalePositionForMidiNote(i)] = true;
            }
        }

        if (!skin)
        {
            return;
        }

        for (auto &a : toneKnobs)
        {
            a->isPlaying = false;
        }

        for (int i = 0; i < scale.count; ++i)
        {
            auto ni = (i + 1) % (scale.count);

            if (notesOn[ni])
            {
                toneEditors[i]->setColour(juce::TextEditor::ColourIds::backgroundColourId,
                                          skin->getColor(clr::ToneLabelBackgroundPlayed));
                toneEditors[i]->setColour(juce::TextEditor::ColourIds::outlineColourId,
                                          skin->getColor(clr::ToneLabelBorderPlayed));
                toneEditors[i]->setColour(juce::TextEditor::ColourIds::focusedOutlineColourId,
                                          skin->getColor(clr::ToneLabelBorderPlayed));
                toneEditors[i]->setColour(juce::TextEditor::ColourIds::textColourId,
                                          skin->getColor(clr::ToneLabelTextPlayed));
                toneEditors[i]->applyColourToAllText(skin->getColor(clr::ToneLabelTextPlayed),
                                                     true);

                toneLabels[i + 1]->setColour(juce::Label::ColourIds::textColourId,
                                             skin->getColor(clr::ToneLabelPlayed));
                toneKnobs[i + 1]->isPlaying = true;

                if (i == scale.count - 1)
                {
                    toneLabels[0]->setColour(juce::Label::ColourIds::textColourId,
                                             skin->getColor(clr::ToneLabelPlayed));
                    toneKnobs[0]->isPlaying = true;
                }
            }
            else
            {
                toneEditors[i]->setColour(juce::TextEditor::ColourIds::backgroundColourId,
                                          skin->getColor(clr::ToneLabelBackground));
                toneEditors[i]->setColour(juce::TextEditor::ColourIds::outlineColourId,
                                          skin->getColor(clr::ToneLabelBorder));
                toneEditors[i]->setColour(juce::TextEditor::ColourIds::focusedOutlineColourId,
                                          skin->getColor(clr::ToneLabelBorder));
                toneEditors[i]->setColour(juce::TextEditor::ColourIds::textColourId,
                                          skin->getColor(clr::ToneLabelText));
                toneEditors[i]->applyColourToAllText(skin->getColor(clr::ToneLabelText), true);

                toneLabels[i + 1]->setColour(juce::Label::ColourIds::textColourId,
                                             skin->getColor(clr::ToneLabel));
                if (i == scale.count - 1)
                    toneLabels[0]->setColour(juce::Label::ColourIds::textColourId,
                                             skin->getColor(clr::ToneLabel));
            }

            toneEditors[i]->repaint();
            toneLabels[i + 1]->repaint();
            toneLabels[0]->repaint();
        }

        toneInterior->repaint();
        toneList->repaint();
        repaint();
    }

    int whichSideOfZero = 0;

    void mouseMove(const juce::MouseEvent &e) override;
    void mouseDown(const juce::MouseEvent &e) override;
    void mouseDrag(const juce::MouseEvent &e) override;
    void mouseDoubleClick(const juce::MouseEvent &e) override;
    void mouseWheelMove(const juce::MouseEvent &event,
                        const juce::MouseWheelDetails &wheel) override;
};

void RadialScaleGraph::paint(juce::Graphics &g)
{
    namespace clr = Colors::TuningOverlay::RadialGraph;

    if (notesOn.size() != scale.count)
    {
        notesOn.clear();
        notesOn.resize(scale.count);

        for (int i = 0; i < scale.count; ++i)
        {
            notesOn[i] = 0;
        }
    }

    g.fillAll(skin->getColor(clr::Background));

    int w = getWidth() - usedForSidebar;
    int h = getHeight();
    float r = std::min(w, h) / 2.1;
    float xo = (w - 2 * r) / 2.0;
    float yo = (h - 2 * r) / 2.0;
    double outerRadiusExtension = 0.4;

    g.saveState();

    screenTransform = juce::AffineTransform()
                          .scaled(1.0 / (1.0 + outerRadiusExtension * 1.1))
                          .scaled(r, -r)
                          .translated(r, r)
                          .translated(xo, yo)
                          .translated(usedForSidebar, 0);
    screenTransformInverted = screenTransform.inverted();

    g.addTransform(screenTransform);

    // We are now in a normal x y 0 1 coordinate system with 0, 0 at the center. Cool

    // So first things first - scan for range.
    double ETInterval = scale.tones.back().cents / scale.count;
    double dIntMin = 0, dIntMax = 0;
    double pCents = 0;
    std::vector<float> intervals; // between tone i and i-1

    for (int i = 0; i < scale.count; ++i)
    {
        auto t = scale.tones[i];
        auto c = t.cents;
        auto in = c - pCents;

        pCents = c;
        intervals.push_back(in);

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

            g.setColour(juce::Colour(110, 110, 120)
                            .interpolatedWith(getLookAndFeel().findColour(
                                                  juce::ResizableWindow::backgroundColourId),
                                              cpos * 0.8));

            float rad = 1.0 + dInterval * i;

            g.drawEllipse(-rad, -rad, 2 * rad, 2 * rad, 0.01);
        }
    }

    for (int i = 0; i < scale.count; ++i)
    {
        double frac = 1.0 * i / (scale.count);
        double hfrac = 0.5 / scale.count;
        double sx = std::sin(frac * 2.0 * juce::MathConstants<double>::pi);
        double cx = std::cos(frac * 2.0 * juce::MathConstants<double>::pi);

        if (notesOn[i])
        {
            g.setColour(juce::Colour(255, 255, 255));
        }
        else
        {
            g.setColour(juce::Colour(110, 110, 120));
        }

        g.drawLine(0, 0, (1.0 + outerRadiusExtension) * sx, (1.0 + outerRadiusExtension) * cx,
                   0.01);

        if (centsShowing)
        {
            if (scale.count > 48)
            {
                // we just don't have room to draw the intervals
            }
            if (scale.count > 18)
            {
                // draw them sideways
                juce::Graphics::ScopedSaveState gs(g);
                auto t =
                    juce::AffineTransform()
                        .scaled(-0.7, 0.7)
                        .rotated(juce::MathConstants<double>::pi)
                        .translated(1.05, 0.0)
                        .rotated((-frac + 0.25 - hfrac) * 2.0 * juce::MathConstants<double>::pi);

                g.addTransform(t);
                g.setColour(juce::Colours::white);
                g.setFont(skin->fontManager->getLatoAtSize(0.1));

                auto msg = fmt::format("{:.2f}", intervals[i]);
                auto tr = juce::Rectangle<float>(0.f, -0.1f, 0.6f, 0.2f);

                g.setColour(juce::Colours::white);
                g.drawText(msg, tr, juce::Justification::centredLeft);
            }
            else
            {
                juce::Graphics::ScopedSaveState gs(g);

                auto t =
                    juce::AffineTransform()
                        .scaled(-0.7, 0.7)
                        .rotated(juce::MathConstants<double>::pi / 2)
                        .translated(1.05, 0.0)
                        .rotated((-frac + 0.25 - hfrac) * 2.0 * juce::MathConstants<double>::pi);

                g.addTransform(t);
                g.setColour(juce::Colours::white);
                g.setFont(skin->fontManager->getLatoAtSize(0.1));

                auto msg = fmt::format("{:.2f}", intervals[i]);
                auto tr = juce::Rectangle<float>(-0.3f, -0.2f, 0.6f, 0.2f);

                g.setColour(juce::Colours::white);
                g.drawText(msg, tr, juce::Justification::centred);
            }
        }

        g.saveState();
        g.addTransform(juce::AffineTransform::rotation((-frac + 0.25) * 2.0 *
                                                       juce::MathConstants<double>::pi));
        g.addTransform(juce::AffineTransform::translation(1.0 + outerRadiusExtension, 0.0));
        g.addTransform(juce::AffineTransform::rotation(juce::MathConstants<double>::pi * 0.5));
        g.addTransform(juce::AffineTransform::scale(-1.0, 1.0));

        if (notesOn[i])
        {
            g.setColour(juce::Colour(255, 255, 255));
        }
        else
        {
            g.setColour(juce::Colour(200, 200, 240));
        }

        // tone labels
        juce::Rectangle<float> textPos(-0.05, -0.115, 0.1, 0.1);
        g.setFont(skin->fontManager->getLatoAtSize(0.075));
        g.drawText(juce::String(i), textPos, juce::Justification::centred, 1);
        g.restoreState();
    }

    // Draw the ring at 1.0
    g.setColour(juce::Colour(255, 255, 255));
    g.drawEllipse(-1, -1, 2, 2, 0.01);

    // Then draw ellipses for each note
    screenHotSpots.clear();

    for (int i = 1; i <= scale.count; ++i)
    {
        double frac = 1.0 * i / (scale.count);
        double sx = std::sin(frac * 2.0 * juce::MathConstants<double>::pi);
        double cx = std::cos(frac * 2.0 * juce::MathConstants<double>::pi);

        auto t = scale.tones[i - 1];
        auto c = t.cents;
        auto expectedC = scale.tones.back().cents / scale.count;

        auto rx = 1.0 + dInterval * (c - expectedC * i) / expectedC;
        float dx = 0.1, dy = 0.1;

        if (i == scale.count)
        {
            dx = 0.15;
            dy = 0.15;
        }

        float x0 = rx * sx - 0.5 * dx, y0 = rx * cx - 0.5 * dy;

        if (notesOn[i])
        {
            g.setColour(juce::Colour(255, 255, 255));
            g.drawLine(sx, cx, rx * sx, rx * cx, 0.03);
        }

        juce::Colour drawColour(200, 200, 200);

        // FIXME - this colormap is bad
        if (rx < 0.99)
        {
            // use a blue here
            drawColour = juce::Colour(200 * (1.0 - 0.6 * rx), 200 * (1.0 - 0.6 * rx), 200);
        }
        else if (rx > 1.01)
        {
            // Use a yellow here
            drawColour = juce::Colour(200, 200, 200 * (rx - 1.0));
        }

        auto originalDrawColor = drawColour;

        if (hotSpotIndex == i - 1)
        {
            drawColour = drawColour.brighter(0.6);
        }

        if (i == scale.count)
        {
            g.setColour(drawColour);
            g.drawLine(sx, cx, rx * sx, rx * cx, 0.01);
            g.fillEllipse(x0, y0, dx, dy);

            if (hotSpotIndex == i - 1)
            {
                auto p = juce::Path();
                if (whichSideOfZero < 0)
                {
                    p.addArc(x0, y0, dx, dy, juce::MathConstants<float>::pi,
                             juce::MathConstants<float>::twoPi);
                }
                else
                {
                    p.addArc(x0, y0, dx, dy, 0, juce::MathConstants<float>::pi);
                }
                g.setColour(juce::Colour(200, 200, 100));
                g.fillPath(p);
            }
        }
        else
        {
            g.setColour(drawColour);

            g.drawLine(sx, cx, rx * sx, rx * cx, 0.01);
            g.fillEllipse(x0, y0, dx, dy);

            if (hotSpotIndex != i - 1)
            {
                g.setColour(drawColour.brighter(0.6));
                g.drawEllipse(x0, y0, dx, dy, 0.01);
            }
        }

        if (notesOn[i % scale.count])
        {
            g.setColour(juce::Colour(255, 255, 255));
            g.drawEllipse(x0, y0, dx, dy, 0.02);
        }

        dx += x0;
        dy += y0;
        screenTransform.transformPoint(x0, y0);
        screenTransform.transformPoint(dx, dy);
        screenHotSpots.push_back(juce::Rectangle<float>(x0, dy, dx - x0, y0 - dy));
    }

    g.restoreState();
}

struct IntervalMatrix : public juce::Component, public Surge::GUI::SkinConsumingComponent
{
    IntervalMatrix(TuningOverlay *o) : overlay(o)
    {
        viewport = std::make_unique<juce::Viewport>();
        intervalPainter = std::make_unique<IntervalPainter>(this);
        viewport->setViewedComponent(intervalPainter.get(), false);

        whatLabel = std::make_unique<juce::Label>("Interval");
        addAndMakeVisible(*whatLabel);

        explLabel = std::make_unique<juce::Label>("Interval");
        explLabel->setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(*explLabel);

        addAndMakeVisible(*viewport);

        setIntervalMode();
    };
    virtual ~IntervalMatrix() = default;

    void setRotationMode()
    {
        whatLabel->setText("Scale Rotation Intervals",
                           juce::NotificationType::dontSendNotification);
        explLabel->setText("If you shift the scale root to note N, show the interval to note M",
                           juce::NotificationType::dontSendNotification);
        intervalPainter->mode = IntervalMatrix::IntervalPainter::ROTATION;

        repaint();
    }

    void setIntervalMode()
    {
        whatLabel->setText("Interval Between Notes", juce::NotificationType::dontSendNotification);
        explLabel->setText(
            "Given any two notes in the loaded scale, show the interval in cents between them",
            juce::NotificationType::dontSendNotification);
        intervalPainter->mode = IntervalMatrix::IntervalPainter::INTERV;
        repaint();
    }

    void setIntervalRelativeMode()
    {
        whatLabel->setText("Interval to Equal Division",
                           juce::NotificationType::dontSendNotification);
        explLabel->setText("Given any two notes in the loaded scale, show the distance to the "
                           "equal division interval",
                           juce::NotificationType::dontSendNotification);
        intervalPainter->mode = IntervalMatrix::IntervalPainter::DIST;

        repaint();
    }

    std::vector<float> rcents;

    void setTuning(const Tunings::Tuning &t)
    {
        tuning = t;
        setNotesOn(bitset);

        float lastc = 0;
        rcents.clear();
        rcents.push_back(0);
        for (auto &t : t.scale.tones)
        {
            rcents.push_back(t.cents);
            lastc = t.cents;
        }
        for (auto &t : t.scale.tones)
        {
            rcents.push_back(t.cents + lastc);
        }
        intervalPainter->setSizeFromTuning();
        intervalPainter->repaint();
    }

    struct IntervalPainter : public juce::Component, public Surge::GUI::SkinConsumingComponent
    {
        enum Mode
        {
            INTERV,
            DIST,
            ROTATION
        } mode{INTERV};
        IntervalPainter(IntervalMatrix *m) : matrix(m) {}

        static constexpr int cellH{14}, cellW{35};
        void setSizeFromTuning()
        {
            auto ic = matrix->tuning.scale.count + 2;
            auto nh = ic * cellH;
            auto nw = ic * cellW;

            setSize(nw, nh);
        }

        void paint(juce::Graphics &g) override
        {
            if (!skin)
                return;

            namespace clr = Colors::TuningOverlay::Interval;
            g.fillAll(skin->getColor(clr::Background));
            auto ic = matrix->tuning.scale.count;
            int mt = ic + (mode == ROTATION ? 1 : 2);
            g.setFont(skin->fontManager->getLatoAtSize(9));
            for (int i = 0; i < mt; ++i)
            {
                bool noi = i > 0 ? matrix->notesOn[i - 1] : false;
                for (int j = 0; j < mt; ++j)
                {
                    bool noj = j > 0 ? matrix->notesOn[j - 1] : false;
                    bool isHovered = false;
                    if ((i == hoverI && j == hoverJ) || (i == 0 && j == hoverJ) ||
                        (i == hoverI && j == 0))
                    {
                        isHovered = true;
                    }
                    auto bx = juce::Rectangle<float>(i * cellW, j * cellH, cellW - 1, cellH - 1);
                    if ((i == 0 || j == 0) && (i + j))
                    {
                        auto no = false;
                        if (i > 0)
                            no = no || matrix->notesOn[i - 1];
                        if (j > 0)
                            no = no || matrix->notesOn[j - 1];

                        if (no)
                            g.setColour(skin->getColor(clr::NoteLabelBackgroundPlayed));
                        else
                            g.setColour(skin->getColor(clr::NoteLabelBackground));
                        g.fillRect(bx);

                        auto lb = std::to_string(i + j - 1);

                        if (isHovered && no)
                            g.setColour(skin->getColor(clr::NoteLabelForegroundHoverPlayed));
                        if (isHovered)
                            g.setColour(skin->getColor(clr::NoteLabelForegroundHover));
                        else if (no)
                            g.setColour(skin->getColor(clr::NoteLabelForegroundPlayed));
                        else
                            g.setColour(skin->getColor(clr::NoteLabelForeground));
                        g.drawText(lb, bx, juce::Justification::centred);
                    }
                    else if (i == j && mode != ROTATION)
                    {
                        g.setColour(juce::Colours::darkgrey);
                        g.fillRect(bx);
                        if (mode == ROTATION && i != 0)
                        {
                            g.setColour(skin->getColor(clr::IntervalSkipped));
                            g.drawText("0", bx, juce::Justification::centred);
                        }
                    }
                    else if (i > j && mode != ROTATION)
                    {
                        auto centsi = 0.0;
                        auto centsj = 0.0;
                        if (i > 1)
                            centsi = matrix->tuning.scale.tones[i - 2].cents;
                        if (j > 1)
                            centsj = matrix->tuning.scale.tones[j - 2].cents;

                        auto cdiff = centsi - centsj;
                        auto disNote = i - j;
                        auto lastTone =
                            matrix->tuning.scale.tones[matrix->tuning.scale.count - 1].cents;
                        auto evenStep = lastTone / matrix->tuning.scale.count;
                        auto desCents = disNote * evenStep;

                        // ToDo: Skin these endpoints
                        if (fabs(cdiff - desCents) < 0.1)
                        {
                            g.setColour(skin->getColor(clr::HeatmapZero));
                        }
                        else if (cdiff < desCents)
                        {
                            // we are flat of even
                            auto dist = std::min((desCents - cdiff) / evenStep, 1.0);
                            auto c1 = skin->getColor(clr::HeatmapNegFar);
                            auto c2 = skin->getColor(clr::HeatmapNegNear);
                            g.setColour(c1.interpolatedWith(c2, 1.0 - dist));
                        }
                        else
                        {
                            auto dist = std::min(-(desCents - cdiff) / evenStep, 1.0);
                            auto b = 1.0 - dist;
                            auto c1 = skin->getColor(clr::HeatmapPosFar);
                            auto c2 = skin->getColor(clr::HeatmapPosNear);
                            g.setColour(c1.interpolatedWith(c2, b));
                        }

                        if (noi && noj)
                        {
                            g.setColour(skin->getColor(clr::NoteLabelBackgroundPlayed));
                        }
                        g.fillRect(bx);

                        auto displayCents = cdiff;
                        if (mode == DIST)
                            displayCents = cdiff - desCents;
                        auto lb = fmt::format("{:.1f}", displayCents);
                        if (isHovered)
                            g.setColour(skin->getColor(clr::IntervalTextHover));
                        else
                            g.setColour(skin->getColor(clr::IntervalText));
                        g.drawText(lb, bx, juce::Justification::centred);
                    }
                    else if (mode == ROTATION && i > 0)
                    {
                        auto centsi = matrix->rcents[j - 1];
                        auto centsj = matrix->rcents[i + j - 1];
                        auto cdiff = centsj - centsi;

                        auto disNote = i;
                        auto lastTone =
                            matrix->tuning.scale.tones[matrix->tuning.scale.count - 1].cents;
                        auto evenStep = lastTone / matrix->tuning.scale.count;
                        auto desCents = disNote * evenStep;

                        if (fabs(cdiff - desCents) < 0.1)
                        {
                            g.setColour(skin->getColor(clr::HeatmapZero));
                        }
                        else if (cdiff < desCents)
                        {
                            // we are flat of even
                            auto dist = std::min((desCents - cdiff) / evenStep, 1.0);
                            auto r = (1.0 - dist);
                            auto c1 = skin->getColor(clr::HeatmapNegFar);
                            auto c2 = skin->getColor(clr::HeatmapNegNear);
                            g.setColour(c1.interpolatedWith(c2, r));
                        }
                        else
                        {
                            auto dist = std::min(-(desCents - cdiff) / evenStep, 1.0);
                            auto b = 1.0 - dist;
                            auto c1 = skin->getColor(clr::HeatmapPosFar);
                            auto c2 = skin->getColor(clr::HeatmapPosNear);
                            g.setColour(c1.interpolatedWith(c2, b));
                        }
                        g.fillRect(bx);

                        if (isHovered)
                            g.setColour(skin->getColor(clr::IntervalTextHover));
                        else
                            g.setColour(skin->getColor(clr::IntervalText));

                        g.drawText(fmt::format("{:.1f}", cdiff), bx, juce::Justification::centred);
                    }
                }
            }
        }

        int hoverI{-1}, hoverJ{-1};

        juce::Point<float> lastMousePos;
        void mouseDown(const juce::MouseEvent &e) override
        {
            if (!Surge::GUI::showCursor(matrix->overlay->storage))
            {
                juce::Desktop::getInstance().getMainMouseSource().enableUnboundedMouseMovement(
                    true);
            }
            lastMousePos = e.position;
        }
        void mouseUp(const juce::MouseEvent &e) override
        {
            if (!Surge::GUI::showCursor(matrix->overlay->storage))
            {
                juce::Desktop::getInstance().getMainMouseSource().enableUnboundedMouseMovement(
                    false);
                auto p = localPointToGlobal(e.mouseDownPosition);
                juce::Desktop::getInstance().getMainMouseSource().setScreenPosition(p);
            }
            // show
        }
        void mouseDrag(const juce::MouseEvent &e) override
        {

            auto dPos = e.position.getY() - lastMousePos.getY();
            dPos = -dPos;
            auto speed = 0.5;
            if (e.mods.isShiftDown())
                speed = 0.05;
            dPos = dPos * speed;
            lastMousePos = e.position;

            if (mode == ROTATION)
            {
                int tonI = (hoverI - 1 + hoverJ - 1) % matrix->tuning.scale.count;
                matrix->overlay->onToneChanged(tonI, matrix->tuning.scale.tones[tonI].cents + dPos);
            }
            else
            {
                auto i = hoverI;
                if (i > 1)
                {
                    auto centsi = matrix->tuning.scale.tones[i - 2].cents + dPos;
                    matrix->overlay->onToneChanged(i - 2, centsi);
                }
            }
        }

        void mouseDoubleClick(const juce::MouseEvent &e) override
        {
            if (mode == ROTATION)
            {
                if (hoverI > 0 && hoverJ > 0)
                {
                    int tonI = (hoverI - 1 + hoverJ - 1) % matrix->tuning.scale.count;
                    matrix->overlay->onToneChanged(
                        tonI, (hoverI + hoverJ - 1) *
                                  matrix->tuning.scale.tones[matrix->tuning.scale.count - 1].cents /
                                  matrix->tuning.scale.count);
                }
            }
            else
            {
                if (hoverI > 1 && hoverI > hoverJ)
                {
                    int tonI = (hoverI - 2) % matrix->tuning.scale.count;
                    matrix->overlay->onToneChanged(
                        tonI, (hoverI - 1) *
                                  matrix->tuning.scale.tones[matrix->tuning.scale.count - 1].cents /
                                  matrix->tuning.scale.count);
                }
            }
        }
        void mouseEnter(const juce::MouseEvent &e) override { repaint(); }

        void mouseExit(const juce::MouseEvent &e) override
        {
            hoverI = -1;
            hoverJ = -1;
            repaint();

            setMouseCursor(juce::MouseCursor::NormalCursor);
        }

        void mouseMove(const juce::MouseEvent &e) override
        {
            if (setupHoverFrom(e.position))
                repaint();
            if (hoverI >= 1 && hoverI <= matrix->tuning.scale.count && hoverJ >= 1 &&
                hoverJ <= matrix->tuning.scale.count && hoverI > hoverJ)
            {
                setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
            }
            else
            {
                setMouseCursor(juce::MouseCursor::NormalCursor);
            }
        }

        bool setupHoverFrom(const juce::Point<float> &here)
        {
            int ohi = hoverI, ohj = hoverJ;

            //  auto bx = juce::Rectangle<int>(i * cellW, j * cellH, cellW - 1, cellH - 1);
            // box x is i*cellW to i*cellW + cellW
            // box x / cellW is i to i + 1
            // floor(box x / cellW ) is i
            hoverI = floor(here.x / cellW);
            hoverJ = floor(here.y / cellH);
            if (ohi != hoverI || ohj != hoverJ)
            {
                return true;
            }
            return false;
        }

        IntervalMatrix *matrix;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IntervalPainter);
    };

    void resized() override
    {
        auto br = getLocalBounds().reduced(2);
        auto tr = br.withHeight(25);
        auto vr = br.withTrimmedTop(25);
        whatLabel->setBounds(tr);
        explLabel->setBounds(tr);
        viewport->setBounds(vr);
        intervalPainter->setSizeFromTuning();
    }

    void onSkinChanged() override
    {
        intervalPainter->setSkin(skin, associatedBitmapStore);
        whatLabel->setFont(skin->fontManager->getLatoAtSize(12, juce::Font::bold));
        explLabel->setFont(skin->fontManager->getLatoAtSize(8));
    }
    std::vector<bool> notesOn;
    std::bitset<128> bitset{0};
    void setNotesOn(const std::bitset<128> &bs)
    {
        bitset = bs;
        notesOn.resize(tuning.scale.count + 1);
        for (int i = 0; i < tuning.scale.count; ++i)
            notesOn[i] = false;
        notesOn[tuning.scale.count] = notesOn[0];

        for (int i = 0; i < 128; ++i)
        {
            if (bitset[i])
            {
                notesOn[tuning.scalePositionForMidiNote(i)] = true;
            }
        }
        intervalPainter->repaint();
        repaint();
    }

    void paint(juce::Graphics &g) override
    {
        if (!skin)
            return;
        g.fillAll(skin->getColor(Colors::TuningOverlay::Interval::Background));
    }

    std::unique_ptr<IntervalPainter> intervalPainter;
    std::unique_ptr<juce::Viewport> viewport;
    std::unique_ptr<juce::Label> whatLabel;
    std::unique_ptr<juce::Label> explLabel;

    Tunings::Tuning tuning;
    TuningOverlay *overlay{nullptr};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IntervalMatrix);
};

void RadialScaleGraph::mouseMove(const juce::MouseEvent &e)
{
    int owso = whichSideOfZero;
    int ohsi = hotSpotIndex;
    hotSpotIndex = -1;
    int h = 0;
    for (auto r : screenHotSpots)
    {
        if (r.contains(e.getPosition().toFloat()))
        {
            hotSpotIndex = h;
            if (h == scale.count - 1)
            {
                if (e.position.x < r.getCentreX())
                {
                    whichSideOfZero = -1;
                }
                else
                {
                    whichSideOfZero = 1;
                }
            }
        }
        h++;
    }

    if (hotSpotIndex >= 0)
        wheelHotSpotIndex = hotSpotIndex;

    if (ohsi != hotSpotIndex || owso != whichSideOfZero)
        repaint();
}
void RadialScaleGraph::mouseDown(const juce::MouseEvent &e)
{
    if (hotSpotIndex == -1)
    {
        centsAtMouseDown = 0;
        angleAtMouseDown = 0;
    }
    else
    {
        centsAtMouseDown = scale.tones[hotSpotIndex].cents;
        angleAtMouseDown = toneKnobs[hotSpotIndex + 1]->angle;
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
        auto speed = 0.7;
        if (e.mods.isShiftDown())
            speed = speed * 0.1;
        dr = dr * speed;

        toneKnobs[hotSpotIndex + 1]->angle = angleAtMouseDown + 100 * dr / dIntervalAtMouseDown;
        toneKnobs[hotSpotIndex + 1]->repaint();
        selfEditGuard++;
        if (hotSpotIndex == scale.count - 1 && whichSideOfZero > 0)
        {
            auto ct = e.mods.isShiftDown() ? 0.05 : 1;
            onScaleRescaled(dr > 0 ? ct : -ct);
        }
        else
        {
            onToneChanged(hotSpotIndex, centsAtMouseDown + 100 * dr / dIntervalAtMouseDown);
        }
        selfEditGuard--;
    }
}

void RadialScaleGraph::mouseDoubleClick(const juce::MouseEvent &e)
{
    if (hotSpotIndex != -1)
    {
        auto newCents = (hotSpotIndex + 1) * scale.tones[scale.count - 1].cents / scale.count;
        toneKnobs[hotSpotIndex + 1]->angle = 0;
        toneKnobs[hotSpotIndex + 1]->repaint();
        selfEditGuard++;
        onToneChanged(hotSpotIndex, newCents);
        selfEditGuard--;
    }
}

void RadialScaleGraph::mouseWheelMove(const juce::MouseEvent &event,
                                      const juce::MouseWheelDetails &wheel)
{
    if (wheelHotSpotIndex != -1)
    {
        float delta = wheel.deltaX - (wheel.isReversed ? 1 : -1) * wheel.deltaY;
        if (delta == 0)
            return;

#if MAC
        float speed = 1.2;
#else
        float speed = 0.42666;
#endif
        if (event.mods.isShiftDown())
        {
            speed = speed / 10;
        }

        // This is calibrated to give us reasonable speed on a 0-1 basis from the slider
        // but in this widget '1' is the small motion so speed it up some
        auto dr = speed * delta;

        toneKnobs[wheelHotSpotIndex + 1]->angle =
            toneKnobs[wheelHotSpotIndex + 1]->angle + 100 * dr / dInterval;
        toneKnobs[wheelHotSpotIndex + 1]->repaint();
        auto newCents = scale.tones[wheelHotSpotIndex].cents + 100 * dr / dInterval;

        selfEditGuard++;
        onToneChanged(wheelHotSpotIndex, newCents);
        selfEditGuard--;
    }
}
void RadialScaleGraph::textEditorReturnKeyPressed(juce::TextEditor &editor)
{
    for (int i = 0; i <= scale.count - 1; ++i)
    {
        if (&editor == toneEditors[i].get())
        {
            selfEditGuard++;
            onToneStringChanged(i, editor.getText().toStdString());
            selfEditGuard--;
        }
    }
}

struct SCLKBMDisplay : public juce::Component,
                       Surge::GUI::SkinConsumingComponent,
                       juce::TextEditor::Listener,
                       juce::CodeDocument::Listener
{
    SCLKBMDisplay(TuningOverlay *o) : overlay(o)
    {
        sclDocument = std::make_unique<juce::CodeDocument>();
        sclDocument->addListener(this);
        sclTokeniser = std::make_unique<SCLKBMTokeniser>();
        scl = std::make_unique<juce::CodeEditorComponent>(*sclDocument, sclTokeniser.get());
        scl->setLineNumbersShown(false);
        scl->setScrollbarThickness(8);
        addAndMakeVisible(*scl);

        kbmDocument = std::make_unique<juce::CodeDocument>();
        kbmDocument->addListener(this);
        kbmTokeniser = std::make_unique<SCLKBMTokeniser>(false);

        kbm = std::make_unique<juce::CodeEditorComponent>(*kbmDocument, kbmTokeniser.get());
        kbm->setLineNumbersShown(false);
        kbm->setScrollbarThickness(8);
        addAndMakeVisible(*kbm);

        auto teProps = [this](const auto &te) {
            te->setJustification((juce::Justification::verticallyCentred));
            // te->setIndents(4, (te->getHeight() - te->getTextHeight()) / 2);
        };

        auto newL = [this](const std::string &s) {
            auto res = std::make_unique<juce::Label>(s, s);
            res->setText(s, juce::dontSendNotification);
            addAndMakeVisible(*res);
            return res;
        };
        evenDivOfL = newL("Divide");
        evenDivOf = std::make_unique<juce::TextEditor>();
        teProps(evenDivOf);
        evenDivOf->setText("2", juce::dontSendNotification);
        addAndMakeVisible(*evenDivOf);

        evenDivIntoL = newL("into");
        evenDivInto = std::make_unique<juce::TextEditor>();
        teProps(evenDivInto);
        evenDivInto->setText("12", juce::dontSendNotification);
        addAndMakeVisible(*evenDivInto);

        evenDivStepsL = newL("steps");

        edoGo = std::make_unique<Surge::Widgets::SelfDrawButton>("Generate");
        edoGo->setStorage(overlay->storage);
        edoGo->setHeightOfOneImage(13);
        edoGo->setSkin(skin, associatedBitmapStore);
        edoGo->onClick = [this]() {
            // FIXME locale
            auto txt = evenDivOf->getText();

            try
            {
                if (txt.contains("."))
                {
                    auto spanct = std::atof(evenDivOf->getText().toRawUTF8());
                    auto num = std::atoi(evenDivInto->getText().toRawUTF8());

                    this->overlay->onNewSCLKBM(Tunings::evenDivisionOfCentsByM(spanct, num).rawText,
                                               kbmDocument->getAllContent().toStdString());
                }
                else if (txt.contains("/"))
                {
                    auto tt = Tunings::toneFromString(txt.toStdString());
                    auto spanct = tt.cents;
                    auto num = std::atoi(evenDivInto->getText().toRawUTF8());

                    this->overlay->onNewSCLKBM(
                        Tunings::evenDivisionOfCentsByM(spanct, num, txt.toStdString()).rawText,
                        kbmDocument->getAllContent().toStdString());
                }
                else
                {
                    auto span = std::atoi(evenDivOf->getText().toRawUTF8());
                    auto num = std::atoi(evenDivInto->getText().toRawUTF8());

                    this->overlay->onNewSCLKBM(Tunings::evenDivisionOfSpanByM(span, num).rawText,
                                               kbmDocument->getAllContent().toStdString());
                }
            }
            catch (const Tunings::TuningError &e)
            {
                overlay->storage->reportError(e.what(), "Tuning Error");
            }
        };
        addAndMakeVisible(*edoGo);

        kbmStartL = newL("Root:");
        kbmStart = std::make_unique<juce::TextEditor>();
        teProps(kbmStart);
        kbmStart->setText("60", juce::dontSendNotification);
        addAndMakeVisible(*kbmStart);

        kbmConstantL = newL("Constant:");
        kbmConstant = std::make_unique<juce::TextEditor>();
        teProps(kbmConstant);
        kbmConstant->setText("69", juce::dontSendNotification);
        addAndMakeVisible(*kbmConstant);

        kbmFreqL = newL("Freq:");
        kbmFreq = std::make_unique<juce::TextEditor>();
        teProps(kbmFreq);
        kbmFreq->setText("440", juce::dontSendNotification);
        addAndMakeVisible(*kbmFreq);

        kbmGo = std::make_unique<Surge::Widgets::SelfDrawButton>("Generate");
        kbmGo->setStorage(overlay->storage);
        kbmGo->setHeightOfOneImage(13);
        kbmGo->setSkin(skin, associatedBitmapStore);
        kbmGo->onClick = [this]() {
            // FIXME locale
            auto start = std::atoi(kbmStart->getText().toRawUTF8());
            auto constant = std::atoi(kbmConstant->getText().toRawUTF8());
            auto freq = std::atof(kbmFreq->getText().toRawUTF8());

            try
            {
                this->overlay->onNewSCLKBM(
                    sclDocument->getAllContent().toStdString(),
                    Tunings::startScaleOnAndTuneNoteTo(start, constant, freq).rawText);
            }
            catch (const Tunings::TuningError &e)
            {
                overlay->storage->reportError(e.what(), "Tuning Error");
            }
        };
        addAndMakeVisible(*kbmGo);
    }

    struct SCLKBMTokeniser : public juce::CodeTokeniser
    {
        enum
        {
            token_Error,
            token_Comment,
            token_Text,
            token_Cents,
            token_Ratio,
            token_Playing
        };

        bool isSCL{false};
        SCLKBMTokeniser(bool s = true) : isSCL(s) {}

        int readNextToken(juce::CodeDocument::Iterator &source) override
        {
            auto firstChar = source.peekNextChar();
            if (firstChar == '!')
            {
                source.skipToEndOfLine();
                return token_Comment;
            }
            if (!isSCL)
            {
                source.skipToEndOfLine();
                return token_Text;
            }
            source.skipWhitespace();
            auto nc = source.nextChar();
            while (nc >= '0' && nc <= '9' && nc)
            {
                nc = source.nextChar();
            }
            source.previousChar(); // in case we are just numbers
            source.skipToEndOfLine();

            auto ln = source.getLine();
            int toneIdx = -1;
            if (toneToLine.find(ln) != toneToLine.end())
            {
                toneIdx = toneToLine[ln];
            }

            if (toneIdx >= 0 && toneIdx < scale.count)
            {
                if (notesOn.size() == scale.count)
                {
                    auto ni = (toneIdx + scale.count + 1) % scale.count;
                    if (notesOn[ni])
                        return token_Playing;
                }
                const auto &tone = scale.tones[toneIdx];
                if (tone.type == Tunings::Tone::kToneCents)
                    return token_Cents;
                if (tone.type == Tunings::Tone::kToneRatio)
                    return token_Ratio;
            }
            return token_Text;
        }

        std::vector<bool> notesOn;
        Tunings::Scale scale;
        std::unordered_map<int, int> toneToLine;
        void setScale(const Tunings::Scale &s)
        {
            scale = s;
            toneToLine.clear();
            int idx = 0;
            for (const auto &t : scale.tones)
            {
                toneToLine[t.lineno] = idx++;
            }
        }

        void setNotesOn(const std::vector<bool> &no) { notesOn = no; }
        juce::CodeEditorComponent::ColourScheme getDefaultColourScheme() override
        {
            struct Type
            {
                const char *name;
                uint32_t colour;
            };

            // clang-format off
            const Type types[] = {
                {"Error", 0xffcc0000},
                {"Comment", 0xffff0000},
                {"Text", 0xFFFF0000},
                {"Cents", 0xFFFF0000},
                {"Ratio", 0xFFFF0000},
                {"Played", 0xFFFF0000 }
            };
            // clang-format on

            juce::CodeEditorComponent::ColourScheme cs;

            for (auto &t : types)
                cs.set(t.name, juce::Colour(t.colour));

            return cs;
        }
    };

    std::vector<bool> notesOn;
    std::bitset<128> bitset{0};
    void setNotesOn(const std::bitset<128> &bs)
    {
        bitset = bs;
        notesOn.resize(tuning.scale.count);
        for (int i = 0; i < tuning.scale.count; ++i)
            notesOn[i] = false;

        for (int i = 0; i < 128; ++i)
        {
            if (bitset[i])
            {
                notesOn[tuning.scalePositionForMidiNote(i)] = true;
            }
        }

        if (sclTokeniser)
            sclTokeniser->setNotesOn(notesOn);
        if (scl)
            scl->retokenise(0, -1);
        repaint();
    }

    std::unique_ptr<juce::CodeDocument> sclDocument, kbmDocument;
    std::unique_ptr<SCLKBMTokeniser> sclTokeniser, kbmTokeniser;

    Tunings::Tuning tuning;

    void setTuning(const Tunings::Tuning &t)
    {
        tuning = t;
        sclTokeniser->setScale(t.scale);
        sclDocument->replaceAllContent(t.scale.rawText);
        kbmDocument->replaceAllContent(t.keyboardMapping.rawText);
        setApplyEnabled(false);
    }

    void resized() override
    {
        auto w = getWidth();
        auto h = getHeight();
        auto b = juce::Rectangle<int>(0, 0, w / 2, h).reduced(3, 3).withTrimmedBottom(20);

        scl->setBounds(b);
        kbm->setBounds(b.translated(w / 2, 0));

        auto r = juce::Rectangle<int>(0, h - 21, w, 20);
        auto s = r.withWidth(w / 2).withTrimmedLeft(2);

        auto nxt = [&s](int p) {
            auto q = s.withWidth(p);
            s = s.withTrimmedLeft(p);
            return q.reduced(0, 2);
        };

        evenDivOfL->setBounds(nxt(37));
        evenDivOf->setBounds(nxt(80));
        evenDivIntoL->setBounds(nxt(30));
        evenDivInto->setBounds(nxt(40));
        evenDivStepsL->setBounds(nxt(30));
        edoGo->setBounds(nxt(60));

        s = r.withTrimmedLeft(w / 2 + 2);
        kbmStartL->setBounds(nxt(30));
        kbmStart->setBounds(nxt(50));
        kbmConstantL->setBounds(nxt(50));
        kbmConstant->setBounds(nxt(50));
        kbmFreqL->setBounds(nxt(30));
        kbmFreq->setBounds(nxt(50));
        s = s.translated(3, 0);
        kbmGo->setBounds(nxt(50));

        auto teProps = [this](const auto &te) {
            te->setFont(skin->fontManager->getFiraMonoAtSize(9));
            te->setJustification((juce::Justification::verticallyCentred));
            te->setIndents(4, (te->getHeight() - te->getTextHeight()) / 2);
        };

        teProps(evenDivOf);
        teProps(evenDivInto);
        teProps(kbmStart);
        teProps(kbmConstant);
        teProps(kbmFreq);
    }

    void setApplyEnabled(bool b);

    void codeDocumentTextInserted(const juce::String &newText, int insertIndex) override
    {
        setApplyEnabled(true);
    }
    void codeDocumentTextDeleted(int startIndex, int endIndex) override { setApplyEnabled(true); }

    void paint(juce::Graphics &g) override
    {
        namespace clr = Colors::TuningOverlay::SCLKBM;
        g.fillAll(skin->getColor(clr::Background));
        g.setColour(skin->getColor(clr::Editor::Border));
        g.drawRect(scl->getBounds().expanded(1), 2);
        g.drawRect(kbm->getBounds().expanded(1), 2);
    }

    void textEditorTextChanged(juce::TextEditor &editor) override { setApplyEnabled(true); }

    void onSkinChanged() override
    {
        namespace clr = Colors::TuningOverlay::SCLKBM::Editor;
        scl->setColour(juce::CodeEditorComponent::ColourIds::backgroundColourId,
                       skin->getColor(clr::Background));
        kbm->setColour(juce::CodeEditorComponent::ColourIds::backgroundColourId,
                       skin->getColor(clr::Background));
        edoGo->setSkin(skin, associatedBitmapStore);
        kbmGo->setSkin(skin, associatedBitmapStore);
        for (auto t : {scl.get(), kbm.get()})
        {
            auto cs = t->getColourScheme();

            cs.set("Comment", skin->getColor(clr::Comment));
            cs.set("Text", skin->getColor(clr::Text));
            cs.set("Cents", skin->getColor(clr::Cents));
            cs.set("Ratio", skin->getColor(clr::Ratio));
            cs.set("Played", skin->getColor(clr::Played));

            t->setColourScheme(cs);
        }

        namespace qclr = Colors::TuningOverlay::RadialGraph;

        for (const auto &r : {evenDivIntoL.get(), evenDivOfL.get(), evenDivStepsL.get(),
                              kbmStartL.get(), kbmConstantL.get(), kbmFreqL.get()})
        {
            r->setColour(juce::Label::textColourId, skin->getColor(qclr::ToneLabelText));
            r->setFont(skin->fontManager->getLatoAtSize(9));
        }

        for (const auto &r :
             {evenDivInto.get(), evenDivOf.get(), kbmStart.get(), kbmConstant.get(), kbmFreq.get()})
        {
            r->setFont(skin->fontManager->getLatoAtSize(9));

            r->setColour(juce::TextEditor::ColourIds::backgroundColourId,
                         skin->getColor(qclr::ToneLabelBackground));
            r->setColour(juce::TextEditor::ColourIds::outlineColourId,
                         skin->getColor(qclr::ToneLabelBorder));
            r->setColour(juce::TextEditor::ColourIds::focusedOutlineColourId,
                         skin->getColor(qclr::ToneLabelBorder));
            r->setColour(juce::TextEditor::ColourIds::textColourId,
                         skin->getColor(qclr::ToneLabelText));
            r->applyColourToAllText(skin->getColor(qclr::ToneLabelText), true);

            // Work around a buglet that the text editor applies fonts ontly to newly inserted
            // text after setFont
            auto t = r->getText();
            r->setText("--", juce::dontSendNotification);
            r->setText(t, juce::dontSendNotification);
        }

        scl->setFont(skin->fontManager->getFiraMonoAtSize(9));
        kbm->setFont(skin->fontManager->getFiraMonoAtSize(9));
    }

    std::function<void(const std::string &scl, const std::string &kbl)> onTextChanged =
        [](auto a, auto b) {};

    std::unique_ptr<juce::CodeEditorComponent> scl;
    std::unique_ptr<juce::CodeEditorComponent> kbm;
    TuningOverlay *overlay{nullptr};

    std::unique_ptr<juce::Label> evenDivOfL, evenDivIntoL, evenDivStepsL;
    std::unique_ptr<juce::TextEditor> evenDivOf, evenDivInto;
    std::unique_ptr<Surge::Widgets::SelfDrawButton> edoGo;

    std::unique_ptr<juce::Label> kbmStartL, kbmConstantL, kbmFreqL;
    std::unique_ptr<juce::TextEditor> kbmStart, kbmConstant, kbmFreq;
    std::unique_ptr<Surge::Widgets::SelfDrawButton> kbmGo;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SCLKBMDisplay);
};

struct TuningControlArea : public juce::Component,
                           public Surge::GUI::SkinConsumingComponent,
                           public Surge::GUI::IComponentTagValue::Listener
{
    enum tags
    {
        tag_select_tab = 0x475200,
        tag_export_html,
        tag_save_scl,
        tag_apply_sclkbm,
        tag_open_library
    };
    TuningOverlay *overlay{nullptr};
    TuningControlArea(TuningOverlay *ol) : overlay(ol)
    {
        setAccessible(true);
        setTitle("Controls");
        setDescription("Controls");
        setFocusContainerType(juce::Component::FocusContainerType::keyboardFocusContainer);
    }

    void resized() override
    {
        if (skin)
            rebuild();
    }

    void rebuild()
    {
        int labelHeight = 12;
        int buttonHeight = 14;
        int numfieldHeight = 12;
        int margin = 2;
        int xpos = 10;

        removeAllChildren();
        auto h = getHeight();

        {
            int marginPos = xpos + margin;
            int btnWidth = 210;
            int ypos = 1 + labelHeight + margin;

            selectL = newL("Edit Mode");
            selectL->setBounds(xpos, 1, 100, labelHeight);
            addAndMakeVisible(*selectL);

            selectS = std::make_unique<Surge::Widgets::MultiSwitchSelfDraw>();
            auto btnrect = juce::Rectangle<int>(marginPos, ypos - 1, btnWidth, buttonHeight);

            selectS->setBounds(btnrect);
            selectS->setStorage(overlay->storage);
            selectS->setLabels({"Scala", "Radial", "Interval", "To Equal", "Rotation"});
            selectS->addListener(this);
            selectS->setDraggable(true);
            selectS->setTag(tag_select_tab);
            selectS->setHeightOfOneImage(buttonHeight);
            selectS->setRows(1);
            selectS->setColumns(5);
            selectS->setDraggable(true);
            selectS->setSkin(skin, associatedBitmapStore);
            selectS->setValue(
                overlay->storage->getPatch().dawExtraState.editor.tuningOverlayState.editMode /
                4.f);
            addAndMakeVisible(*selectS);
            xpos += btnWidth + 10;
        }

        {
            int marginPos = xpos + margin;
            int btnWidth = 65;
            int ypos = 1 + labelHeight + margin;

            actionL = newL("Actions");
            actionL->setBounds(xpos, 1, 100, labelHeight);
            addAndMakeVisible(*actionL);

            auto ma = [&](const std::string &l, tags t) {
                auto res = std::make_unique<Surge::Widgets::MultiSwitchSelfDraw>();
                auto btnrect = juce::Rectangle<int>(marginPos, ypos - 1, btnWidth, buttonHeight);

                res->setBounds(btnrect);
                res->setStorage(overlay->storage);
                res->setLabels({l});
                res->addListener(this);
                res->setTag(t);
                res->setHeightOfOneImage(buttonHeight);
                res->setRows(1);
                res->setColumns(1);
                res->setDraggable(false);
                res->setSkin(skin, associatedBitmapStore);
                res->setValue(0);
                return res;
            };

            savesclS = ma("Save Scale", tag_save_scl);
            addAndMakeVisible(*savesclS);
            marginPos += btnWidth + 5;

            exportS = ma("Export HTML", tag_export_html);
            addAndMakeVisible(*exportS);
            marginPos += btnWidth + 5;

            libraryS = ma("Tuning Library", tag_open_library);
            addAndMakeVisible(*libraryS);
            marginPos += btnWidth + 5;

            applyS = ma("Apply", tag_apply_sclkbm);
            addAndMakeVisible(*applyS);
            applyS->setEnabled(false);
            xpos += btnWidth + 5;
        }
    }

    std::unique_ptr<juce::Label> newL(const std::string &s)
    {
        auto res = std::make_unique<juce::Label>(s, s);
        res->setText(s, juce::dontSendNotification);
        res->setFont(skin->fontManager->getLatoAtSize(9, juce::Font::bold));
        res->setColour(juce::Label::textColourId, skin->getColor(Colors::MSEGEditor::Text));
        return res;
    }

    void valueChanged(GUI::IComponentTagValue *c) override
    {
        auto tag = (tags)(c->getTag());
        switch (tag)
        {
        case tag_select_tab:
        {
            int m = c->getValue() * 4;
            overlay->showEditor(m);
            selectS->repaint();
        }
        break;
        case tag_save_scl:
        {
            if (applyS->isEnabled())
            {
                overlay->storage->reportError(
                    "You have unapplied changes in your SCL/KBM. Please apply them before saving!",
                    "SCL Save Error");
                break;
            }
            fileChooser = std::make_unique<juce::FileChooser>("Save SCL", juce::File(), "*.scl");
            fileChooser->launchAsync(
                juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles |
                    juce::FileBrowserComponent::warnAboutOverwriting,
                [this](const juce::FileChooser &c) {
                    auto result = c.getResults();

                    if (result.isEmpty() || result.size() > 1)
                    {
                        return;
                    }

                    auto fsp = fs::path{result[0].getFullPathName().toStdString()};
                    fsp = fsp.replace_extension(".scl");

                    std::ofstream ofs(fsp);
                    if (ofs.is_open())
                    {
                        ofs << overlay->tuning.scale.rawText;
                        ofs.close();
                    }
                    else
                    {
                        overlay->storage->reportError("Unable to save SCL file", "SCL File Error");
                    }
                });
        }
        break;
        case tag_open_library:
        {
            auto path = overlay->storage->datapath / "tuning_library";
            Surge::GUI::openFileOrFolder(path);
        }
        break;
        case tag_export_html:
        {
            if (overlay && overlay->editor)
            {
                overlay->editor->showHTML(overlay->editor->tuningToHtml());
            }
        }
        break;
        case tag_apply_sclkbm:
        {
            if (applyS->isEnabled())
            {
                if (overlay->storage && overlay->editor)
                    overlay->editor->undoManager()->pushTuning(overlay->storage->currentTuning);
                auto *sck = overlay->sclKbmDisplay.get();
                sck->onTextChanged(sck->sclDocument->getAllContent().toStdString(),
                                   sck->kbmDocument->getAllContent().toStdString());
                applyS->setEnabled(false);
                applyS->repaint();
            }
        }
        break;
        }
    }

    std::unique_ptr<juce::Label> selectL, intervalL, actionL;
    std::unique_ptr<Surge::Widgets::MultiSwitchSelfDraw> selectS, intervalS, exportS, savesclS,
        libraryS, applyS;
    std::unique_ptr<juce::FileChooser> fileChooser;

    void paint(juce::Graphics &g) override { g.fillAll(skin->getColor(Colors::MSEGEditor::Panel)); }

    void onSkinChanged() override { rebuild(); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TuningControlArea);
};

void SCLKBMDisplay::setApplyEnabled(bool b)
{
    overlay->controlArea->applyS->setEnabled(b);
    overlay->controlArea->applyS->repaint();
}

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

    sclKbmDisplay = std::make_unique<SCLKBMDisplay>(this);
    sclKbmDisplay->onTextChanged = [this](const std::string &s, const std::string &k) {
        this->onNewSCLKBM(s, k);
    };

    radialScaleGraph = std::make_unique<RadialScaleGraph>();
    radialScaleGraph->onToneChanged = [this](int note, double d) { this->onToneChanged(note, d); };
    radialScaleGraph->onToneStringChanged = [this](int note, const std::string &d) {
        this->onToneStringChanged(note, d);
    };
    radialScaleGraph->onScaleRescaled = [this](double d) { this->onScaleRescaled(d); };
    radialScaleGraph->onScaleRescaledAbsolute = [this](double d) {
        this->onScaleRescaledAbsolute(d);
    };

    intervalMatrix = std::make_unique<IntervalMatrix>(this);

    controlArea = std::make_unique<TuningControlArea>(this);
    addAndMakeVisible(*controlArea);

    addChildComponent(*sclKbmDisplay);
    sclKbmDisplay->setVisible(true);

    addChildComponent(*radialScaleGraph);
    radialScaleGraph->setVisible(false);

    addChildComponent(*intervalMatrix);
    intervalMatrix->setVisible(false);
}

void TuningOverlay::setStorage(SurgeStorage *s) { storage = s; }

TuningOverlay::~TuningOverlay() = default;

void TuningOverlay::resized()
{
    auto t = getTransform().inverted();
    auto h = getHeight();
    auto w = getWidth();

    int kbWidth = 87;
    int ctrlHeight = 35;

    t.transformPoint(w, h);

    tuningKeyboardTable->setBounds(0, 0, kbWidth, h);
    tuningKeyboardTable->getHeader().setColour(
        juce::TableHeaderComponent::ColourIds::highlightColourId, juce::Colours::transparentWhite);

    auto contentArea = juce::Rectangle<int>(kbWidth, 0, w - kbWidth, h - ctrlHeight);

    sclKbmDisplay->setBounds(contentArea);
    radialScaleGraph->setBounds(contentArea);
    intervalMatrix->setBounds(contentArea);
    controlArea->setBounds(kbWidth, h - ctrlHeight, w - kbWidth, ctrlHeight);

    // it's a bit of a hack to put this here but by this point I'm all set up
    if (storage)
    {
        auto mcoff = Surge::Storage::getUserDefaultValue(storage, Surge::Storage::MiddleC, 1);
        tuningKeyboardTableModel->setMiddleCOff(mcoff);

        showEditor(storage->getPatch().dawExtraState.editor.tuningOverlayState.editMode);
    }
}

void TuningOverlay::showEditor(int which)
{
    jassert(which >= 0 && which <= 5);
    if (which == 0)
        controlArea->applyS->setVisible(true);
    else
        controlArea->applyS->setVisible(false);
    sclKbmDisplay->setVisible(which == 0);
    radialScaleGraph->setVisible(which == 1);
    intervalMatrix->setVisible(which >= 2);
    if (which == 2)
    {
        intervalMatrix->setIntervalMode();
    }
    if (which == 3)
    {
        intervalMatrix->setIntervalRelativeMode();
    }
    if (which == 4)
    {
        intervalMatrix->setRotationMode();
    }

    if (storage)
    {
        storage->getPatch().dawExtraState.editor.tuningOverlayState.editMode = which;
    }
}

void TuningOverlay::onToneChanged(int tone, double newCentsValue)
{
    if (storage)
    {
        editor->undoManager()->pushTuning(storage->currentTuning);
        storage->currentScale.tones[tone].type = Tunings::Tone::kToneCents;
        storage->currentScale.tones[tone].cents = newCentsValue;
        recalculateScaleText();
    }
}

void TuningOverlay::onScaleRescaled(double scaleBy)
{
    if (!storage)
        return;

    editor->undoManager()->pushTuning(storage->currentTuning);

    /*
     * OK so we want a 1 cent move on top so that is top -> top+1 for scaleBy = 1
     * top ( 1 + x * scaleBy ) = top+1
     * x = top+1/top-1 since scaleBy is 1 in non shift mode
     */
    auto tCents = std::max(storage->currentScale.tones[storage->currentScale.count - 1].cents, 1.0);
    double sFactor = (tCents + 1) / tCents - 1;

    double scale = (1.0 + sFactor * scaleBy);
    for (auto &t : storage->currentScale.tones)
    {
        t.type = Tunings::Tone::kToneCents;
        t.cents *= scale;
    }
    recalculateScaleText();
}

void TuningOverlay::onScaleRescaledAbsolute(double riTo)
{
    if (!storage)
        return;

    editor->undoManager()->pushTuning(storage->currentTuning);

    /*
     * OK so we want a 1 cent move on top so that is top -> top+1 for scaleBy = 1
     * top ( 1 + x * scaleBy ) = top+1
     * x = top+1/top-1 since scaleBy is 1 in non shift mode
     */
    auto tCents = std::max(storage->currentScale.tones[storage->currentScale.count - 1].cents, 1.0);

    double scale = riTo / tCents;
    for (auto &t : storage->currentScale.tones)
    {
        t.type = Tunings::Tone::kToneCents;
        t.cents *= scale;
    }
    recalculateScaleText();
}

void TuningOverlay::onToneStringChanged(int tone, const std::string &newStringValue)
{
    if (storage)
    {
        editor->undoManager()->pushTuning(storage->currentTuning);
        try
        {
            auto parsed = Tunings::toneFromString(newStringValue);
            storage->currentScale.tones[tone] = parsed;
            recalculateScaleText();
        }
        catch (Tunings::TuningError &e)
        {
            storage->reportError(e.what(), "Tuning Tone Conversion");
        }
    }
}

void TuningOverlay::onNewSCLKBM(const std::string &scl, const std::string &kbm)
{
    if (!storage)
        return;

    editor->undoManager()->pushTuning(storage->currentTuning);

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

void TuningOverlay::setMidiOnKeys(const std::bitset<128> &keys)
{
    tuningKeyboardTableModel->notesOn = keys;
    tuningKeyboardTable->repaint();
    radialScaleGraph->setNotesOn(keys);
    intervalMatrix->setNotesOn(keys);
    sclKbmDisplay->setNotesOn(keys);
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
        auto str = oss.str();
        storage->retuneToScale(Tunings::parseSCLData(str));
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
    radialScaleGraph->setTuning(t);
    intervalMatrix->setTuning(t);
    repaint();
}

void TuningOverlay::onSkinChanged()
{
    tuningKeyboardTableModel->setSkin(skin, associatedBitmapStore);
    tuningKeyboardTable->repaint();

    sclKbmDisplay->setSkin(skin, associatedBitmapStore);
    radialScaleGraph->setSkin(skin, associatedBitmapStore);
    radialScaleGraph->storage = storage;

    intervalMatrix->setSkin(skin, associatedBitmapStore);

    controlArea->setSkin(skin, associatedBitmapStore);
}

void TuningOverlay::onTearOutChanged(bool isTornOut) { doDnD = isTornOut; }
bool TuningOverlay::isInterestedInFileDrag(const juce::StringArray &files)
{
    if (!doDnD)
        return false;
    if (editor)
        return editor->juceEditor->isInterestedInFileDrag(files);
    return false;
}
void TuningOverlay::filesDropped(const juce::StringArray &files, int x, int y)
{
    if (!doDnD)
        return;
    if (editor)
        editor->juceEditor->filesDropped(files, x, y);
}

} // namespace Overlays
} // namespace Surge
