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

#ifndef SURGE_XT_XMLCONFIGUREDMENUS_H
#define SURGE_XT_XMLCONFIGUREDMENUS_H

#include "WidgetBaseMixin.h"
#include "SurgeJUCEHelpers.h"
#include "FxPresetAndClipboardManager.h"

#include "juce_gui_basics/juce_gui_basics.h"

#include <utility>

class SurgeStorage;
class SurgeGUIEditor;
class SurgeImage;
class OscillatorStorage;
class FxStorage;

namespace Surge
{
namespace Widgets
{
/*
 * This is a more direct port of CSnapshotMenu than the other widgets since most of
 * the code is state management and the like.
 */
struct XMLMenuPopulator
{
    virtual Surge::GUI::IComponentTagValue *asControlValueInterface() = 0;
    virtual Surge::GUI::IComponentTagValue::Listener *getControlListener() = 0;

    virtual void populate();
    virtual void loadSnapshot(int type, TiXmlElement *e, int idx){};

    SurgeStorage *storage{nullptr};
    void setStorage(SurgeStorage *s) { storage = s; }

    int selectedIdx{-1};
    std::string selectedName;

    juce::PopupMenu menu;

    /*
     * New Data Structures for Presets and what not
     */
    void scanXMLPresets();
    void scanXMLPresetForType(TiXmlElement *typeElement, const std::vector<std::string> &path);
    struct Item
    {
        std::vector<std::string> pathElements;
        std::string name;
        TiXmlElement *xmlElement{nullptr};
        int itemType{0};
        fs::path path{};
        bool isUser{false};
        bool isSeparator{false};
        bool isSectionHeader{false};
        bool hasColumnBreak{false};

        /*
         * I know this isn't very generalized and I could template it and subclass
         * and use this for N other controls. But I wont
         */
        Surge::Storage::FxUserPreset::Preset fxPreset;
        bool hasFxUserPreset{false};
    };
    virtual void scanExtraPresets() {}
    std::vector<Item> allPresets;
    virtual void loadByIndex(const std::string &name, int idx)
    {
        auto q = allPresets[idx];
        if (q.xmlElement)
        {
            loadSnapshot(q.itemType, q.xmlElement, idx);
            selectedIdx = idx;
            if (getControlListener())
                getControlListener()->valueChanged(asControlValueInterface());
        }
        selectedIdx = idx;
    }

    void jogBy(int dir)
    {
        if (dir == 0)
            return;
        auto idx = selectedIdx;
        do
        {
            idx += dir;
            if (idx < 0)
                idx = allPresets.size() - 1;

            if (idx >= (int)allPresets.size())
                idx = 0;
        } while (allPresets[idx].isSeparator || allPresets[idx].isSectionHeader);
        loadByIndex(allPresets[idx].name, idx);
    }
    int maxIdx;
    char mtype[64];
};

struct OscillatorMenu : public juce::Component,
                        public XMLMenuPopulator,
                        public WidgetBaseMixin<OscillatorMenu>
{
    OscillatorMenu();
    ~OscillatorMenu();

    void loadSnapshot(int type, TiXmlElement *e, int idx) override;
    void populate() override;

    Surge::GUI::IComponentTagValue *asControlValueInterface() override { return this; };
    Surge::GUI::IComponentTagValue::Listener *getControlListener() override
    {
        return firstListenerOfType<Surge::GUI::IComponentTagValue::Listener>();
    };

    float getValue() const override { return 0; }
    void setValue(float f) override {}

    void mouseDown(const juce::MouseEvent &event) override;
    void mouseWheelMove(const juce::MouseEvent &event,
                        const juce::MouseWheelDetails &wheel) override;

    void mouseEnter(const juce::MouseEvent &event) override;
    void mouseExit(const juce::MouseEvent &event) override;

    bool isHovered{false};
    Surge::GUI::WheelAccumulationHelper wheelAccumulationHelper;

    OscillatorStorage *osc{nullptr};
    void setOscillatorStorage(OscillatorStorage *o);

    SurgeImage *bg{}, *bgHover{};
    void setBackgroundDrawable(SurgeImage *b) { bg = b; };
    void setHoverBackgroundDrawable(SurgeImage *bgh) { bgHover = bgh; }

    void paint(juce::Graphics &g) override;

    void startHover(const juce::Point<float> &) override
    {
        isHovered = true;
        repaint();
    }

    void endHover() override
    {
        if (stuckHover)
            return;

        isHovered = false;
        repaint();
    }

    bool isCurrentlyHovered() override { return isHovered; }

    bool keyPressed(const juce::KeyPress &key) override;

    void focusGained(juce::Component::FocusChangeType cause) override
    {
        startHover(getBounds().getBottomLeft().toFloat());
    }

    void focusLost(juce::Component::FocusChangeType cause) override { endHover(); }

    bool text_allcaps{false};
    juce::Font::FontStyleFlags font_style{juce::Font::plain};

    int font_size{8}, text_hoffset{0}, text_voffset{0};
    juce::Justification text_align{juce::Justification::centred};

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OscillatorMenu);
};

struct FxMenu : public juce::Component, public XMLMenuPopulator, public WidgetBaseMixin<FxMenu>
{
    FxMenu();

    Surge::GUI::IComponentTagValue *asControlValueInterface() override { return this; };
    Surge::GUI::IComponentTagValue::Listener *getControlListener() override
    {
        return firstListenerOfType<Surge::GUI::IComponentTagValue::Listener>();
    };

    float getValue() const override { return 0; }
    void setValue(float f) override {}

    void scanExtraPresets() override;

    void mouseDown(const juce::MouseEvent &event) override;

    void mouseEnter(const juce::MouseEvent &event) override;
    void mouseExit(const juce::MouseEvent &event) override;

    Surge::GUI::WheelAccumulationHelper wheelAccumulationHelper;
    void mouseWheelMove(const juce::MouseEvent &event,
                        const juce::MouseWheelDetails &wheel) override;

    bool isHovered{false};

    void paint(juce::Graphics &g) override;

    void loadSnapshot(int type, TiXmlElement *e, int idx) override;
    void populateForContext(bool isCalledInEffectChooser);
    void populate() override { populateForContext(false); };

    FxStorage *fx{nullptr}, *fxbuffer{nullptr};
    void setFxStorage(FxStorage *s);
    void setFxBuffer(FxStorage *s) { fxbuffer = s; }

    int current_fx;
    void setCurrentFx(int i) { current_fx = i; }

    static Surge::FxClipboard::Clipboard fxClipboard;
    void copyFX();
    void pasteFX();
    void saveFX();

    void loadByIndex(const std::string &name, int index) override;
    void loadUserPreset(const Surge::Storage::FxUserPreset::Preset &p);

    SurgeImage *bg{}, *bgHover{};
    void setBackgroundDrawable(SurgeImage *b) { bg = b; };
    void setHoverBackgroundDrawable(SurgeImage *bgh) { bgHover = bgh; }

    void startHover(const juce::Point<float> &) override
    {
        isHovered = true;
        repaint();
    }

    void endHover() override
    {
        if (stuckHover)
            return;

        isHovered = false;
        repaint();
    }

    bool isCurrentlyHovered() override { return isHovered; }

    bool keyPressed(const juce::KeyPress &key) override;

    void focusGained(juce::Component::FocusChangeType cause) override
    {
        startHover(getBounds().getBottomLeft().toFloat());
        repaint();
    }

    void focusLost(juce::Component::FocusChangeType cause) override
    {
        endHover();
        repaint();
    }

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FxMenu);
};
} // namespace Widgets
} // namespace Surge
#endif // SURGE_XT_XMLCONFIGUREDMENUS_H
