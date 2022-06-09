/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "SurgeSynthEditor.h"
#include "SurgeSynthProcessor.h"
#include "SurgeImageStore.h"
#include "SurgeImage.h"
#include "SurgeGUIEditor.h"
#include "plugin_type_extensions/SurgeSynthFlavorExtensions.h"
#include "SurgeJUCELookAndFeel.h"
#include "RuntimeFont.h"
#include <version.h>

struct VKeyboardWheel : public juce::Component
{
    std::function<void(int)> onValueChanged = [](int f) {};
    bool snapBack{false};
    bool unipolar{true};
    int range{127};
    int value{0};
    void paint(juce::Graphics &g) override
    {
        auto wheelSz = getLocalBounds().reduced(2, 3);

        g.setColour(findColour(SurgeJUCELookAndFeel::SurgeColourIds::wheelBgId));
        g.fillRect(wheelSz);
        g.setColour(findColour(SurgeJUCELookAndFeel::SurgeColourIds::wheelBorderId));
        g.drawRect(wheelSz.expanded(1, 1));

        float p = 1.0 * value / range;

        if (!unipolar)
        {
            p = 1.0 * (value + range) / (2 * range);
        }

        // y direction is flipped
        p = 1 - p;

        float cp = wheelSz.getY() + p * (wheelSz.getHeight() - 4);
        auto r = wheelSz.withHeight(2).translated(0, cp - 2).reduced(1, 0);

        g.setColour(findColour(SurgeJUCELookAndFeel::SurgeColourIds::wheelValueId));
        g.fillRect(r);
    }

    void valueFromY(float y)
    {
        auto wheelSz = getLocalBounds().reduced(1, 2);
        auto py = std::clamp(y, 1.f * wheelSz.getY(), 1.f * wheelSz.getY() + wheelSz.getHeight());
        py = (py - wheelSz.getY()) / wheelSz.getHeight();
        py = 1 - py;

        if (unipolar)
            value = py * range;
        else
            value = 2 * py * range - range;
        onValueChanged(value);
    }

    void mouseDown(const juce::MouseEvent &event) override
    {
        valueFromY(event.position.y);
        repaint();
    }

    void mouseDrag(const juce::MouseEvent &event) override
    {
        valueFromY(event.position.y);
        repaint();
    }

    void mouseUp(const juce::MouseEvent &event) override
    {
        if (snapBack)
        {
            value = 0;
            onValueChanged(value);
        }
        repaint();
    }
};

struct VKeyboardSus : public juce::Component
{
    std::function<void(int)> onValueChanged = [](int f) {};
    bool isOn{false};
    void paint(juce::Graphics &g) override
    {
        auto wheelSz = getLocalBounds().reduced(1, 2);

        if (isOn)
            g.setColour(findColour(SurgeJUCELookAndFeel::SurgeColourIds::wheelValueId));
        else
            g.setColour(findColour(SurgeJUCELookAndFeel::SurgeColourIds::wheelBgId));
        g.fillRect(wheelSz);
        g.setColour(findColour(SurgeJUCELookAndFeel::SurgeColourIds::wheelBorderId));
        g.drawRect(wheelSz.expanded(1, 1));
    }
    void mouseUp(const juce::MouseEvent &event) override
    {
        isOn = !isOn;
        onValueChanged(isOn * 127);
        repaint();
    }
};

//==============================================================================
SurgeSynthEditor::SurgeSynthEditor(SurgeSynthProcessor &p)
    : juce::AudioProcessorEditor(&p), processor(p)
{
    surgeLF = std::make_unique<SurgeJUCELookAndFeel>(&(processor.surge->storage));

    juce::LookAndFeel::setDefaultLookAndFeel(surgeLF.get());

    addKeyListener(this);

    adapter = std::make_unique<SurgeGUIEditor>(this, processor.surge.get());

    auto mcValue = Surge::Storage::getUserDefaultValue(&(this->processor.surge->storage),
                                                       Surge::Storage::MiddleC, 1);

    keyboard = std::make_unique<juce::MidiKeyboardComponent>(
        processor.midiKeyboardState, juce::MidiKeyboardComponent::Orientation::horizontalKeyboard);
    keyboard->setVelocity(midiKeyboardVelocity, true);
    keyboard->setOctaveForMiddleC(5 - mcValue);
    keyboard->setKeyPressBaseOctave(midiKeyboardOctave);
    keyboard->setLowestVisibleKey(24);
    // this makes VKB always receive keyboard input (except when we focus on any typeins, of course)
    keyboard->setWantsKeyboardFocus(false);

    auto w = std::make_unique<VKeyboardWheel>();
    w->snapBack = true;
    w->unipolar = false;
    w->range = 8196;
    w->onValueChanged = [this](auto f) {
        processor.midiFromGUI.push(
            SurgeSynthProcessor::midiR(SurgeSynthProcessor::midiR::PITCHWHEEL, f));
    };
    pitchwheel = std::move(w);

    auto m = std::make_unique<VKeyboardWheel>();
    m->onValueChanged = [this](auto f) {
        processor.midiFromGUI.push(
            SurgeSynthProcessor::midiR(SurgeSynthProcessor::midiR::MODWHEEL, f));
    };
    modwheel = std::move(m);

    auto sp = std::make_unique<VKeyboardSus>();
    sp->onValueChanged = [this](auto f) {
        processor.midiFromGUI.push(
            SurgeSynthProcessor::midiR(SurgeSynthProcessor::midiR::SUSPEDAL, f));
    };
    suspedal = std::move(sp);

    tempoTypein = std::make_unique<juce::TextEditor>("Tempo");
    tempoTypein->setFont(adapter->currentSkin->fontManager->getLatoAtSize(11));
    tempoTypein->setInputRestrictions(3, "0123456789");
    tempoTypein->setSelectAllWhenFocused(true);
    tempoTypein->onReturnKey = [this]() {
        // this is thread sloppy
        float newT = std::atof(tempoTypein->getText().toRawUTF8());

        processor.standaloneTempo = newT;
        tempoTypein->giveAwayKeyboardFocus();
    };

    tempoLabel = std::make_unique<juce::Label>("Tempo", "Tempo");

    addChildComponent(*keyboard);
    addChildComponent(*pitchwheel);
    addChildComponent(*modwheel);
    addChildComponent(*suspedal);
    addChildComponent(*tempoLabel);
    addChildComponent(*tempoTypein);

    drawExtendedControls = adapter->getShowVirtualKeyboard();

    bool addTempo = processor.wrapperType == juce::AudioProcessor::wrapperType_Standalone;
    int yExtra = 0;

    if (drawExtendedControls)
    {
        keyboard->setVisible(true);
        tempoLabel->setVisible(addTempo);
        tempoTypein->setVisible(addTempo);

        yExtra = extraYSpaceForVirtualKeyboard;
    }
    else
    {
        keyboard->setVisible(false);
        tempoLabel->setVisible(false);
        tempoTypein->setVisible(false);

        yExtra = 0;
    }

    auto rg = BlockRezoom(this);
    setSize(BASE_WINDOW_SIZE_X, BASE_WINDOW_SIZE_Y + yExtra);
    // add the bottom right corner resizer only for VST2
    setResizable(true, processor.wrapperType == juce::AudioProcessor::wrapperType_VST);

    adapter->open(nullptr);

    idleTimer = std::make_unique<IdleTimer>(this);
    idleTimer->startTimer(1000 / 60);

    SurgeSynthEditorSpecificExtensions(this, adapter.get());
}

SurgeSynthEditor::~SurgeSynthEditor()
{
    idleTimer->stopTimer();
    adapter->close();

    if (adapter->bitmapStore)
    {
        adapter->bitmapStore->clearAllLoadedBitmaps();
    }

    adapter.reset(nullptr);
}

void SurgeSynthEditor::handleAsyncUpdate() {}

void SurgeSynthEditor::paint(juce::Graphics &g)
{
    g.fillAll(findColour(SurgeJUCELookAndFeel::SurgeColourIds::tempoBackgroundId));
}

void SurgeSynthEditor::idle() { adapter->idle(); }

void SurgeSynthEditor::reapplySurgeComponentColours()
{
    tempoLabel->setColour(juce::Label::textColourId,
                          findColour(SurgeJUCELookAndFeel::SurgeColourIds::tempoLabelId));

    tempoTypein->setColour(
        juce::TextEditor::backgroundColourId,
        findColour(SurgeJUCELookAndFeel::SurgeColourIds::tempoTypeinBackgroundId));
    tempoTypein->setColour(juce::TextEditor::outlineColourId,
                           findColour(SurgeJUCELookAndFeel::SurgeColourIds::tempoTypeinBorderId));
    tempoTypein->setColour(juce::TextEditor::focusedOutlineColourId,
                           findColour(SurgeJUCELookAndFeel::SurgeColourIds::tempoTypeinBorderId));
    tempoTypein->setColour(
        juce::TextEditor::highlightColourId,
        findColour(SurgeJUCELookAndFeel::SurgeColourIds::tempoTypeinHighlightId));
    tempoTypein->setColour(juce::TextEditor::highlightedTextColourId,
                           findColour(SurgeJUCELookAndFeel::SurgeColourIds::tempoTypeinTextId));
    tempoTypein->setColour(juce::TextEditor::textColourId,
                           findColour(SurgeJUCELookAndFeel::SurgeColourIds::tempoTypeinTextId));
    tempoTypein->applyColourToAllText(
        findColour(SurgeJUCELookAndFeel::SurgeColourIds::tempoTypeinTextId), true);

    for (auto *p = getParentComponent(); p != nullptr; p = p->getParentComponent())
    {
        if (auto dw = dynamic_cast<juce::DocumentWindow *>(p))
        {
            dw->setName("Surge XT");

            if (processor.wrapperType == juce::AudioProcessor::wrapperType_Standalone)
            {
                dw->setColour(juce::DocumentWindow::backgroundColourId,
                              findColour(SurgeJUCELookAndFeel::SurgeColourIds::topWindowBorderId));
            }
        }
    }

    repaint();
}

void SurgeSynthEditor::resized()
{
    drawExtendedControls = adapter->getShowVirtualKeyboard();

    auto w = getWidth();
    auto h = getHeight() - (drawExtendedControls
                                ? 0.01 * adapter->getZoomFactor() * extraYSpaceForVirtualKeyboard
                                : 0);
    auto wR = 1.0 * w / adapter->getWindowSizeX();
    auto hR = 1.0 * h / adapter->getWindowSizeY();

    auto ar =
        1.f * adapter->getWindowSizeX() /
        (adapter->getWindowSizeY() + (drawExtendedControls ? extraYSpaceForVirtualKeyboard : 0));
    if (getConstrainer())
        getConstrainer()->setFixedAspectRatio(ar);

    auto zfn = std::min(wR, hR);
    if (wR < 1 && hR < 1)
        zfn = std::max(wR, hR);
    if ((wR - 1) * (hR - 1) < 0)
        zfn = std::min(zfn, 1.0);

    zfn = 100.0 * zfn / adapter->getZoomFactor();

    float applyZoomFactor = adapter->getZoomFactor() * 0.01;
    if (!rezoomGuard)
        applyZoomFactor *= zfn;

    bool addTempo = processor.wrapperType == juce::AudioProcessor::wrapperType_Standalone;

    if (drawExtendedControls)
    {
        auto y = adapter->getWindowSizeY();
        auto x = addTempo ? 50 : 0;
        auto wheels = 32;
        auto margin = 6;
        int tempoHeight = 14, typeinHeight = 18, yOffset = -2;
        int tempoBlockHeight = tempoHeight + typeinHeight;
        int tempoBlockYPos = ((extraYSpaceForVirtualKeyboard - tempoBlockHeight) / 2) + yOffset;

        auto xf = juce::AffineTransform().scaled(applyZoomFactor);
        auto r = juce::Rectangle<int>(x + wheels + margin, y,
                                      adapter->getWindowSizeX() - x - wheels - margin,
                                      extraYSpaceForVirtualKeyboard);
        keyboard->setBounds(r);
        keyboard->setTransform(xf);
        keyboard->setVisible(true);

        auto pmr = juce::Rectangle<int>(x, y, wheels / 2, extraYSpaceForVirtualKeyboard - 10);
        pitchwheel->setBounds(pmr);
        pitchwheel->setTransform(xf);
        pitchwheel->setVisible(true);
        pmr = pmr.translated((wheels / 2) + margin / 3, 0);
        modwheel->setBounds(pmr);
        modwheel->setTransform(xf);
        modwheel->setVisible(true);

        auto smr = juce::Rectangle<int>(x, y + extraYSpaceForVirtualKeyboard - 10,
                                        wheels + margin / 3, 10);
        suspedal->setBounds(smr);
        suspedal->setTransform(xf);
        suspedal->setVisible(true);

        if (addTempo)
        {
            tempoLabel->setBounds(4, y + tempoBlockYPos, x - 8, tempoHeight);
            tempoLabel->setFont(
                adapter->currentSkin->fontManager->getLatoAtSize(9, juce::Font::bold));
            tempoLabel->setJustificationType(juce::Justification::centred);
            tempoLabel->setTransform(xf);
            tempoLabel->setVisible(addTempo);

            tempoTypein->setBounds(4, y + tempoBlockYPos + tempoHeight, x - 8, typeinHeight);
            tempoTypein->setText(
                std::to_string((int)(processor.surge->storage.temposyncratio * 120)));
            tempoTypein->setFont(adapter->currentSkin->fontManager->getLatoAtSize(11));
            tempoTypein->setIndents(4, 0);
            tempoTypein->setJustification(juce::Justification::centred);
            tempoTypein->setTransform(xf);
            tempoTypein->setVisible(addTempo);
        }
    }
    else
    {
        keyboard->setVisible(false);
        tempoLabel->setVisible(false);
        tempoTypein->setVisible(false);
    }

    if (zfn != 1.0 && rezoomGuard == 0)
    {
        auto br = BlockRezoom(this);
        adapter->setZoomFactor(round(adapter->getZoomFactor() * zfn), false);
    }
}

void SurgeSynthEditor::parentHierarchyChanged() { reapplySurgeComponentColours(); }

void SurgeSynthEditor::IdleTimer::timerCallback() { ed->idle(); }

void SurgeSynthEditor::populateForStreaming(SurgeSynthesizer *s)
{
    if (adapter)
        adapter->populateDawExtraState(s);
}

void SurgeSynthEditor::populateFromStreaming(SurgeSynthesizer *s)
{
    if (adapter)
        adapter->loadFromDAWExtraState(s);
}

bool SurgeSynthEditor::isInterestedInFileDrag(const juce::StringArray &files)
{
    if (files.size() != 1)
        return false;

    for (auto i = files.begin(); i != files.end(); ++i)
    {
        if (adapter->canDropTarget(i->toStdString()))
            return true;
    }
    return false;
}

void SurgeSynthEditor::filesDropped(const juce::StringArray &files, int x, int y)
{
    if (files.size() != 1)
        return;

    for (auto i = files.begin(); i != files.end(); ++i)
    {
        if (adapter->canDropTarget(i->toStdString()))
            adapter->onDrop(i->toStdString());
    }
}

void SurgeSynthEditor::beginParameterEdit(Parameter *p)
{
    auto par = processor.paramsByID[processor.surge->idForParameter(p)];
    par->beginChangeGesture();
}

void SurgeSynthEditor::endParameterEdit(Parameter *p)
{
    auto par = processor.paramsByID[processor.surge->idForParameter(p)];
    par->endChangeGesture();
}

void SurgeSynthEditor::beginMacroEdit(long macroNum)
{
    auto par = processor.macrosById[macroNum];
    par->beginChangeGesture();
}

void SurgeSynthEditor::endMacroEdit(long macroNum)
{
    auto par = processor.macrosById[macroNum];
    par->endChangeGesture();
}

#if LINUX
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

juce::PopupMenu SurgeSynthEditor::hostMenuFor(Parameter *p)
{
    auto par = processor.paramsByID[processor.surge->idForParameter(p)];

    if (auto *c = getHostContext())
        if (auto menuInfo = c->getContextMenuForParameterIndex(par))
            return menuInfo->getEquivalentPopupMenu();

    return juce::PopupMenu();
}

juce::PopupMenu SurgeSynthEditor::hostMenuForMacro(int macro)
{
    auto par = processor.macrosById[macro];

    if (auto *c = getHostContext())
        if (auto menuInfo = c->getContextMenuForParameterIndex(par))
            return menuInfo->getEquivalentPopupMenu();

    return juce::PopupMenu();
}

#if LINUX
#pragma GCC diagnostic pop
#endif

bool SurgeSynthEditor::keyPressed(const juce::KeyPress &key, juce::Component *orig)
{
    /*
     * So, sigh. On Linux/Windows the keyStateChanged event of the parent isn't suppressed
     * when the keyStateChange is false. But the MIDI keyboard rescans on state change.
     * So what we need to do is: forward all keystate trues (which will come before a
     * keypress but only if the keypress is not in a text edit) but only forward keystate
     * false if I have sent at least one key through this fallthrough mechanism. So:
     */
    if (adapter->getShowVirtualKeyboard())
    {
        auto textChar = key.getTextCharacter();

        // shift VKB note input one octave up
        if (textChar == 'c')
        {
            midiKeyboardOctave = std::clamp(midiKeyboardOctave + 1, 0, 9);
            keyboard->setKeyPressBaseOctave(midiKeyboardOctave);

            return true;
        }

        // shift VKB note input one octave down
        if (textChar == 'x')
        {
            midiKeyboardOctave = std::clamp(midiKeyboardOctave - 1, 0, 9);
            keyboard->setKeyPressBaseOctave(midiKeyboardOctave);

            return true;
        }

        // set VKB velocity to basic dynamic levels (ppp, pp, p, mp, mf, f, ff, fff)
        if (textChar >= '1' && textChar <= '8')
        {
            float const velJump = 16.f / 127.f;

            // juce::getTextCharacter() returns ASCII code of the char
            // so subtract the first one we need to get our factor
            midiKeyboardVelocity = std::clamp(velJump * (textChar - '1' + 1), 0.f, 1.f);
            keyboard->setVelocity(midiKeyboardVelocity, true);

            return true;
        }

        // reduce VKB velocity for QWERTY by 10%
        if (textChar == '9')
        {
            midiKeyboardVelocity = std::clamp(midiKeyboardVelocity - 0.1f, 0.f, 1.f);
            keyboard->setVelocity(midiKeyboardVelocity, true);

            return true;
        }

        // increase VKB velocity for QWERTY by 10%
        if (textChar == '0')
        {
            midiKeyboardVelocity = std::clamp(midiKeyboardVelocity + 0.1f, 0.f, 1.f);
            keyboard->setVelocity(midiKeyboardVelocity, true);

            return true;
        }

        if (adapter->shouldForwardKeysToVKB() && orig != keyboard.get())
        {
            return keyboard->keyPressed(key);
        }
    }

    return false;
}

bool SurgeSynthEditor::keyStateChanged(bool isKeyDown, juce::Component *originatingComponent)
{
    if (adapter->getShowVirtualKeyboard() && adapter->shouldForwardKeysToVKB() &&
        originatingComponent != keyboard.get())
    {
        return keyboard->keyStateChanged(isKeyDown);
    }

    return false;
}

void SurgeSynthEditor::setPitchModSustainGUI(int pitch, int mod, int sus)
{
    auto pw = dynamic_cast<VKeyboardWheel *>(pitchwheel.get());
    if (pw)
    {
        pw->value = pitch;
        pw->repaint();
    }
    auto mw = dynamic_cast<VKeyboardWheel *>(modwheel.get());
    if (mw)
    {
        mw->value = mod;
        mw->repaint();
    }
    auto sw = dynamic_cast<VKeyboardSus *>(suspedal.get());
    if (sw)
    {
        sw->isOn = sus > 64;
        sw->repaint();
    }
}