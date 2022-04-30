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

#ifndef SURGE_XT_TYPEINPARAMEDITOR_H
#define SURGE_XT_TYPEINPARAMEDITOR_H

#include "Parameter.h"
#include "SurgeStorage.h"
#include "ModulationSource.h"
#include "SkinSupport.h"

#include "juce_gui_basics/juce_gui_basics.h"

class SurgeGUIEditor;

namespace Surge
{
namespace Overlays
{
struct TypeinParamEditor : public juce::Component,
                           public Surge::GUI::SkinConsumingComponent,
                           public juce::TextEditor::Listener
{
    TypeinParamEditor();
    void paint(juce::Graphics &g) override;

    SurgeGUIEditor *editor{nullptr};
    void setSurgeGUIEditor(SurgeGUIEditor *e) { editor = e; }
    void grabFocus();
    juce::Component *returnFocusComp{nullptr};
    void setReturnFocusTarget(juce::Component *that) { returnFocusComp = that; }
    void doReturnFocus();
    juce::Rectangle<int> getRequiredSize();
    void setBoundsToAccompany(const juce::Rectangle<int> &controlRect,
                              const juce::Rectangle<int> &parentRect);
    void resized() override;

    void visibilityChanged() override;

    Parameter *p{nullptr};
    void setEditedParam(Parameter *pin) { p = pin; };
    bool isMod{false};
    modsources ms{ms_original};
    int modScene{-1}, modidx{0};
    void setModDepth01(bool isMod, modsources ms, int modScene, int modidx)
    {
        this->isMod = isMod;
        this->ms = ms;
        this->modScene = modScene;
        this->modidx = modidx;
    }

    std::string mainLabel;
    void setMainLabel(const std::string &s)
    {
        mainLabel = s;
        setTitle("Edit " + mainLabel);
        setDescription("Edit " + mainLabel);
    }
    std::string primaryVal, secondaryVal;
    void setValueLabels(const std::string &pri, const std::string &sec)
    {
        primaryVal = pri;
        secondaryVal = sec;
    }

    std::string modbyLabel, errorToDisplay;
    void setModByLabel(const std::string &by) { modbyLabel = by; }

    virtual bool handleTypein(const std::string &value, std::string &errMsg);

    void setEditableText(const std::string &et) { textEd->setText(et, juce::dontSendNotification); }

    enum TypeinMode
    {
        Inactive,
        Param,
        Control
    } typeinMode{Inactive};
    void setTypeinMode(TypeinMode t) { typeinMode = t; }
    TypeinMode getTypeinMode() const { return typeinMode; }

    std::unique_ptr<juce::TextEditor> textEd;

    void onSkinChanged() override;
    bool wasInputInvalid{false};

    // Text editor listener methods
    void textEditorReturnKeyPressed(juce::TextEditor &editor) override;
    void textEditorEscapeKeyPressed(juce::TextEditor &editor) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TypeinParamEditor);
};

struct TypeinLambdaEditor : public TypeinParamEditor
{
    TypeinLambdaEditor(std::function<bool(const std::string &)> c) : callback(c){};
    bool handleTypein(const std::string &value, std::string &errMsg) override
    {
        return callback(value);
    }
    std::function<bool(const std::string &)> callback;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TypeinLambdaEditor);
};
} // namespace Overlays
} // namespace Surge
#endif // SURGE_XT_TYPEINPARAMEDITOR_H
