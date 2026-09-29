// SPDX-License-Identifier: GPL-3.0-or-later
// Included inside BrowserPeer. Browser callbacks always revalidate focus and generation.
    Component::SafePointer<Component> inputComponent;
    String inputText;
    int inputGeneration = 0;
    bool inputActive = false, inputComposing = false;
    struct InputFormat { Range<int> range; int style, thickness; };
    std::vector<InputFormat> inputFormats;
    TextInputTarget *activeInput()
    {
        if (!inputActive || inputComponent == nullptr || !isFocused()) return nullptr;
        auto *target = findCurrentTextInputTarget();
        return target == dynamic_cast<TextInputTarget *>(inputComponent.getComponent()) ? target : nullptr;
    }
    void finishInputComposition()
    {
        if (inputComposing)
        {
            inputComposing = false;
            browserCompositionTarget = nullptr;
            browserCompositionComponent = nullptr;
            if (auto *target = dynamic_cast<TextInputTarget *>(inputComponent.getComponent()))
                target->beginInputMethodTransaction();
        }
        inputFormats.clear();
        dirty = true;
    }
    void syncInput()
    {
        const bool nativeText = EM_ASM_INT({ return document.activeElement?.dataset.jucePeer === UTF8ToString($0)
            && document.activeElement instanceof HTMLTextAreaElement; }, canvasId.toRawUTF8());
        auto *target = isFocused() && !nativeText ? findCurrentTextInputTarget() : nullptr;
        auto *focused = Component::getCurrentlyFocusedComponent();
        if (target && target->getKeyboardType() == TextInputTarget::passwordKeyboard)
            target = nullptr;
        if (!target)
        {
            if (inputActive)
            {
                finishInputComposition();
                inputActive = false;
                ++inputGeneration;
                inputComponent = nullptr;
                inputText = {};
                EM_ASM({ SurgeTextInput.detach($0); }, inputPeerId);
            }
            return;
        }
        if (!inputActive || inputComponent.getComponent() != focused)
        {
            finishInputComposition();
            inputComponent = focused;
            inputActive = true;
            ++inputGeneration;
        }
        inputText = target->getTextInRange({0, target->getTotalNumChars()});
        const auto selection = target->getHighlightedRegion();
        auto rectangle = [](Rectangle<int> r) {
            return var(Array<var>{r.getX(), r.getY(), r.getWidth(), r.getHeight()});
        };
        auto *state = new DynamicObject();
        state->setProperty("text", inputText);
        state->setProperty("start", selection.getStart());
        state->setProperty("end", selection.getEnd());
        state->setProperty("control", rectangle(focused->getScreenBounds()));
        state->setProperty("caret", rectangle(focused->localAreaToGlobal(target->getCaretRectangle())));
        const auto json = JSON::toString(var(state), true);
        EM_ASM({ SurgeTextInput.sync($0, $1, JSON.parse(UTF8ToString($2))); },
               inputPeerId, inputGeneration, json.toRawUTF8());
    }
    void paintInput()
    {
        auto *target = activeInput();
        if (!target || inputFormats.empty()) return;
        Graphics g(backing);
        g.addTransform(AffineTransform::scale(float(pixelScale)));
        g.reduceClipRegion(component.getLocalArea(inputComponent.getComponent(), inputComponent->getLocalBounds()));
        g.setColour(inputComponent->findColour(TextEditor::textColourId, true));
        for (const auto &format : inputFormats)
            for (auto rectangle : target->getTextBounds(format.range))
            {
                const auto r = component.getLocalArea(inputComponent.getComponent(), rectangle).toFloat();
                const auto y = r.getBottom() - 1.f;
                const auto thickness = format.thickness == 2 ? 2.f : 1.f;
                if (format.style == 2 || format.style == 3)
                {
                    const float dashes[]{format.style == 2 ? 1.f : 4.f, 2.f};
                    g.drawDashedLine({r.getX(), y, r.getRight(), y}, dashes, 2, thickness);
                }
                else if (format.style == 4)
                {
                    Path path; path.startNewSubPath(r.getX(), y);
                    for (float x = r.getX() + 1; x < r.getRight(); ++x)
                        path.lineTo(x, y + std::sin((x - r.getX()) * 1.5f));
                    g.strokePath(path, PathStrokeType(thickness));
                }
                else g.drawLine(r.getX(), y, r.getRight(), y, thickness);
            }
    }
  public:
    static BrowserPeer *inputPeer(int id)
    {
        for (int i = 0; i < ComponentPeer::getNumPeers(); ++i)
            if (auto *peer = dynamic_cast<BrowserPeer *>(ComponentPeer::getPeer(i)))
                if (peer->inputPeerId == id) return peer;
        return nullptr;
    }
    bool updateInput(int generation, int start, int end, const String &text, int a, int b)
    {
        auto *target = activeInput();
        if (generation != inputGeneration || !target) return false;
        if (inputText != target->getTextInRange({0, target->getTotalNumChars()}) ||
            start < 0 || end < start || end > target->getTotalNumChars())
        {
            syncInput();
            return false;
        }
        const bool composing = isBrowserCompositionActive(target);
        if (!composing) target->beginInputMethodTransaction();
        target->setHighlightedRegion({start, end});
        target->insertTextAtCaret(text);
        const auto count = target->getTotalNumChars();
        a = jlimit(0, count, a); b = jlimit(0, count, b);
        target->setHighlightedRegion({jmin(a,b), jmax(a,b)});
        if (!composing) target->beginInputMethodTransaction();
        syncInput();
        dirty = true;
        return true;
    }
    void compositionInput(int generation, bool begin)
    {
        auto *target = activeInput();
        if (generation != inputGeneration || !target) return;
        if (begin)
        {
            ++browserCompositionSerial;
            browserCompositionTarget = nullptr;
            browserCompositionComponent = nullptr;
            target->beginInputMethodTransaction();
            browserCompositionTarget = target;
            browserCompositionComponent = inputComponent;
            inputComposing = true;
        }
        else finishInputComposition();
    }
    String characterInputBounds(int generation, int index)
    {
        auto *target = activeInput();
        if (generation != inputGeneration || !target) return "[0,0,0,0]";
        index = jlimit(0, target->getTotalNumChars(), index);
        auto r = target->getTextBounds({index, jmin(index + 1, target->getTotalNumChars())}).getBounds();
        if (r.isEmpty()) r = target->getCaretRectangleForCharIndex(index);
        r = inputComponent->localAreaToGlobal(r);
        return JSON::toString(var(Array<var>{r.getX(),r.getY(),r.getWidth(),r.getHeight()}),true);
    }
    void formatInput(int generation, int start, int end, int style, int thickness)
    {
        if (generation != inputGeneration || !activeInput()) return;
        if (start < 0) inputFormats.clear();
        else if (style && thickness && end >= start)
            inputFormats.push_back({{start,end},style,thickness});
        dirty = true;
    }
  private:
