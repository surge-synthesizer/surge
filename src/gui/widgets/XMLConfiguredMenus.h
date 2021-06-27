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

#include <JuceHeader.h>

#include <utility>
#include "WidgetBaseMixin.h"
#include "SurgeJUCEHelpers.h"
#include "FxPresetAndClipboardManager.h"

class SurgeStorage;
class SurgeGUIEditor;
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
    virtual bool loadSnapshotByIndex(int idx);

    SurgeStorage *storage{nullptr};
    void setStorage(SurgeStorage *s) { storage = s; }

    int selectedIdx;
    std::string selectedName;

    juce::PopupMenu menu;

    void populateSubmenuFromTypeElement(TiXmlElement *typeElement, juce::PopupMenu &parent,
                                        int &idx, bool isTopLevel = false);
    virtual void addToTopLevelTypeMenu(TiXmlElement *typeElement, juce::PopupMenu &subMenu,
                                       int &idx)
    {
    }
    virtual void setMenuStartHeader(TiXmlElement *typeElement, juce::PopupMenu &subMenu) {}

    char mtype[16] = {0};
    std::map<int, int> firstSnapshotByType;

    struct LoadArg
    {
        LoadArg(int i, TiXmlElement *e) : type(i), el(e), actionType(XML) {}
        LoadArg(int i, std::function<void()> f)
            : type(i), callback(std::move(f)), actionType(LAMBDA)
        {
        }
        int type{-1};
        TiXmlElement *el{nullptr};
        std::function<void()> callback{[]() {}};
        enum Type
        {
            NONE,
            XML,
            LAMBDA
        } actionType{NONE};
    };
    std::vector<LoadArg> loadArgsByIndex;
    int maxIdx;
};

struct OscillatorMenu : public juce::Component,
                        public XMLMenuPopulator,
                        public WidgetBaseMixin<OscillatorMenu>
{
    OscillatorMenu();
    void loadSnapshot(int type, TiXmlElement *e, int idx) override;

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
    void setOscillatorStorage(OscillatorStorage *o) { osc = o; }

    juce::Drawable *bg{}, *bgHover{};
    void setBackgroundDrawable(juce::Drawable *b) { bg = b; };
    void setHoverBackgroundDrawable(juce::Drawable *bgh) { bgHover = bgh; }

    void paint(juce::Graphics &g) override;

    bool text_allcaps{false};
    juce::Font::FontStyleFlags font_style{juce::Font::plain};

    int font_size{8}, text_hoffset{0}, text_voffset{0};
    juce::Justification text_align{juce::Justification::centred};
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

    void mouseDown(const juce::MouseEvent &event) override;

    void mouseEnter(const juce::MouseEvent &event) override;
    void mouseExit(const juce::MouseEvent &event) override;

    bool isHovered{false};
    Surge::GUI::WheelAccumulationHelper wheelAccumulationHelper;

    void paint(juce::Graphics &g) override;

    void loadSnapshot(int type, TiXmlElement *e, int idx) override;
    void populate() override;

    void addToTopLevelTypeMenu(TiXmlElement *typeElement, juce::PopupMenu &subMenu,
                               int &idx) override;
    FxStorage *fx{nullptr}, *fxbuffer{nullptr};
    void setFxStorage(FxStorage *s) { fx = s; }
    void setFxBuffer(FxStorage *s) { fxbuffer = s; }

    int current_fx;
    void setCurrentFx(int i) { current_fx = i; }

    static Surge::FxClipboard::Clipboard fxClipboard;
    void copyFX();
    void pasteFX();
    void saveFX();

    void setMenuStartHeader(TiXmlElement *typeElement, juce::PopupMenu &subMenu) override;

    void loadUserPreset(const Surge::FxUserPreset::Preset &p);

    juce::Drawable *bg{}, *bgHover{};
    void setBackgroundDrawable(juce::Drawable *b) { bg = b; };
    void setHoverBackgroundDrawable(juce::Drawable *bgh) { bgHover = bgh; }
};
} // namespace Widgets
} // namespace Surge
#endif // SURGE_XT_XMLCONFIGUREDMENUS_H
