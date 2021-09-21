/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2020 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

#include "SurgeGUIEditor.h"

#include "overlays/ModulationEditor.h"
#include "overlays/PatchDBViewer.h"
#include "overlays/PatchStoreDialog.h"
#include "overlays/LuaEditors.h"
#include "overlays/TuningOverlays.h"
#include "overlays/WaveShaperAnalysis.h"
#include "overlays/OverlayWrapper.h"
#include "widgets/MainFrame.h"
#include "widgets/WaveShaperSelector.h"

std::unique_ptr<Surge::Overlays::OverlayComponent> SurgeGUIEditor::makeStorePatchDialog()
{
    auto npc = Surge::Skin::Connector::NonParameterConnection::STORE_PATCH_DIALOG;
    auto conn = Surge::Skin::Connector::connectorByNonParameterConnection(npc);
    auto skinCtrl = currentSkin->getOrCreateControlForConnector(conn);

    std::string name = synth->storage.getPatch().name;
    std::string category = synth->storage.getPatch().category;
    std::string author = synth->storage.getPatch().author;
    std::string comments = synth->storage.getPatch().comment;

    auto defaultAuthor = Surge::Storage::getUserDefaultValue(
        &(this->synth->storage), Surge::Storage::DefaultPatchAuthor, "");
    auto defaultComment = Surge::Storage::getUserDefaultValue(
        &(this->synth->storage), Surge::Storage::DefaultPatchComment, "");
    auto oldAuthor = std::string("");

    if (!Surge::Storage::isValidUTF8(defaultAuthor))
    {
        defaultAuthor = "";
    }
    if (!Surge::Storage::isValidUTF8(defaultComment))
    {
        defaultComment = "";
    }

    if (author == "" && defaultAuthor != "")
    {
        author = defaultAuthor;
    }

    if (author != "" && defaultAuthor != "")
    {
        if (_stricmp(author.c_str(), defaultAuthor.c_str()))
        {
            oldAuthor = author;
            author = defaultAuthor;
        }
    }

    if (comments == "" && defaultComment != "")
    {
        comments = defaultComment;
    }

    if (oldAuthor != "")
    {
        if (comments == "")
            comments += "Original patch by " + oldAuthor;
        else
            comments += " (Original patch by " + oldAuthor + ")";
    }

    auto pb = std::make_unique<Surge::Overlays::PatchStoreDialog>();
    pb->setSkin(currentSkin);
    pb->setName(name);
    pb->setAuthor(author);
    pb->setCategory(category);
    pb->setComment(comments);
    pb->setTags(synth->storage.getPatch().tags);
    pb->setSurgeGUIEditor(this);

    pb->setEnclosingParentTitle("Store Patch");
    pb->setEnclosingParentPosition(skinCtrl->getRect());
    pb->setHasIndependentClose(false);
    return pb;
}

std::unique_ptr<Surge::Overlays::OverlayComponent> SurgeGUIEditor::createOverlay(OverlayTags olt)
{
    switch (olt)
    {
    case PATCH_BROWSER:
    {
        auto pt = std::make_unique<Surge::Overlays::PatchDBViewer>(this, &(this->synth->storage));

        auto npc = Surge::Skin::Connector::NonParameterConnection::PATCH_BROWSER;
        auto conn = Surge::Skin::Connector::connectorByNonParameterConnection(npc);
        auto skinCtrl = currentSkin->getOrCreateControlForConnector(conn);
        auto yPos = skinCtrl->y + skinCtrl->h - 1;
        auto xBuf = 25;
        auto w = getWindowSizeX() - xBuf * 2;
        auto h = getWindowSizeY() - yPos - xBuf;
        pt->setEnclosingParentPosition(juce::Rectangle<int>(xBuf, yPos, w, h));
        pt->setEnclosingParentTitle("Patch Database");
        return pt;
    }
    break;
    case MSEG_EDITOR:
    {
        auto lfo_id = modsource_editor[current_scene] - ms_lfo1;
        msegIsOpenFor = lfo_id;
        msegIsOpenInScene = current_scene;

        auto lfodata = &synth->storage.getPatch().scene[current_scene].lfo[lfo_id];
        auto ms = &synth->storage.getPatch().msegs[current_scene][lfo_id];
        auto mse = std::make_unique<Surge::Overlays::MSEGEditor>(
            &(synth->storage), lfodata, ms, &msegEditState[current_scene][lfo_id], currentSkin,
            bitmapStore);
        mse->onModelChanged = [this]() {
            if (lfoDisplayRepaintCountdown == 0)
            {
                lfoDisplayRepaintCountdown = 2;
            }
        };

        std::string title = modsource_names[modsource_editor[current_scene]];
        title += " Editor";
        Surge::Storage::findReplaceSubstring(title, std::string("LFO"), std::string("MSEG"));

        auto npc = Surge::Skin::Connector::NonParameterConnection::MSEG_EDITOR_WINDOW;
        auto conn = Surge::Skin::Connector::connectorByNonParameterConnection(npc);
        auto skinCtrl = currentSkin->getOrCreateControlForConnector(conn);

        mse->setEnclosingParentTitle(title);
        mse->setEnclosingParentPosition(skinCtrl->getRect());

        return mse;
    }
    break;
    case FORMULA_EDITOR:
    {
        auto npc = Surge::Skin::Connector::NonParameterConnection::MSEG_EDITOR_WINDOW;
        auto conn = Surge::Skin::Connector::connectorByNonParameterConnection(npc);
        auto skinCtrl = currentSkin->getOrCreateControlForConnector(conn);

        auto lfo_id = modsource_editor[current_scene] - ms_lfo1;
        auto fs = &synth->storage.getPatch().formulamods[current_scene][lfo_id];

        auto pt = std::make_unique<Surge::Overlays::FormulaModulatorEditor>(
            this, &(this->synth->storage),
            &synth->storage.getPatch().scene[current_scene].lfo[lfo_id], fs, currentSkin);
        pt->setSkin(currentSkin, bitmapStore);

        std::string title = modsource_names[modsource_editor[current_scene]];
        title += " Editor";
        Surge::Storage::findReplaceSubstring(title, std::string("LFO"), std::string("Formula"));

        pt->setEnclosingParentPosition(skinCtrl->getRect());
        pt->setEnclosingParentTitle(title);
        return pt;
    }
    case STORE_PATCH:
        return makeStorePatchDialog();
        break;
    case TUNING_EDITOR:
    {
        int w = 750, h = 500;
        auto px = (getWindowSizeX() - w) / 2;
        auto py = (getWindowSizeY() - h) / 2;
        auto r = juce::Rectangle<int>(px, py, w, h);

        auto pt = std::make_unique<Surge::Overlays::TuningOverlay>();
        pt->setStorage(&(this->synth->storage));
        pt->setSkin(currentSkin, bitmapStore);
        pt->setTuning(synth->storage.currentTuning);
        // pt->addScaleTextEditedListener(this);
        pt->setEnclosingParentPosition(juce::Rectangle<int>(px, py, w, h));
        pt->setEnclosingParentTitle("Tuning Editor");
        return pt;
    }
    break;
    case WAVETABLESCRIPTING_EDITOR:
    {
        int w = 800, h = 520;
        auto px = (getWindowSizeX() - w) / 2;
        auto py = (getWindowSizeY() - h) / 2;
        auto r = juce::Rectangle<int>(px, py, w, h);

        auto os = &synth->storage.getPatch().scene[current_scene].osc[current_osc[current_scene]];

        auto pt = std::make_unique<Surge::Overlays::WavetableEquationEditor>(
            this, &(this->synth->storage), os, currentSkin);
        pt->setSkin(currentSkin, bitmapStore);
        pt->setEnclosingParentPosition(juce::Rectangle<int>(px, py, w, h));
        pt->setEnclosingParentTitle("Wavetable Script Editor");
        return pt;
    }
    break;

    case WAVESHAPER_ANALYZER:
    {
        auto pt = std::make_unique<Surge::Overlays::WaveShaperAnalysis>(&(this->synth->storage));
        pt->setSkin(currentSkin, bitmapStore);

        auto npc = Surge::Skin::Connector::NonParameterConnection::ANALYZE_WAVESHAPE;
        auto conn = Surge::Skin::Connector::connectorByNonParameterConnection(npc);
        auto skinCtrl = currentSkin->getOrCreateControlForConnector(conn);
        auto b = skinCtrl->getRect();

        auto w = 300;
        auto h = 160;
        auto c = b.getCentreX() - w / 2;
        auto p = juce::Rectangle<int>(0, 0, w, h).withX(c).withY(b.getBottom() + 2);
        pt->setEnclosingParentPosition(p);
        pt->setEnclosingParentTitle("Waveshaper Analysis");
        pt->setWSType(synth->storage.getPatch().scene[current_scene].wsunit.type.val.i);
        return pt;
    }
    break;

    case MODULATION_EDITOR:
    {
        auto pt = std::make_unique<Surge::Overlays::ModulationEditor>(this, this->synth);
        pt->setEnclosingParentTitle("Modulation List");
        pt->setEnclosingParentPosition(juce::Rectangle<int>(50, 50, 750, 450));
        return pt;
    }
    break;
    default:
        break;
    }
    return nullptr;
}
void SurgeGUIEditor::showOverlay(OverlayTags olt,
                                 std::function<void(Surge::Overlays::OverlayComponent *)> setup)
{
    auto ol = createOverlay(olt);
    // copy these before the std::move below
    auto t = ol->getEnclosingParentTitle();
    auto r = ol->getEnclosingParentPosition();
    auto c = ol->getHasIndependentClose();
    setup(ol.get());

    std::function<void()> onClose = []() {};

    switch (olt)
    {
    case MSEG_EDITOR:
        broadcastMSEGState();
        // no break on purpose
    case FORMULA_EDITOR:
        onClose = [this]() {
            if (lfoEditSwitch)
            {
                lfoEditSwitch->setValue(0.0);
                lfoEditSwitch->asJuceComponent()->repaint();
            }
        };
        break;
    case MODULATION_EDITOR:
        onClose = [this]() { frame->repaint(); };
        break;
    case WAVESHAPER_ANALYZER:
        onClose = [this]() { this->synth->refresh_editor = true; };
        break;
    default:
        break;
    }
    addJuceEditorOverlay(std::move(ol), t, olt, r, c, onClose);

    switch (olt)
    {
    case MSEG_EDITOR:
    case FORMULA_EDITOR:
        if (lfoEditSwitch)
        {
            lfoEditSwitch->setValue(1.0);
            lfoEditSwitch->asJuceComponent()->repaint();
        }
        break;
    case MODULATION_EDITOR:
        frame->repaint();
        break;
    default:
        break;
    }
}
void SurgeGUIEditor::closeOverlay(OverlayTags olt)
{
    switch (olt)
    {
    case MSEG_EDITOR:
        broadcastMSEGState();
        // no break on purpose
    case FORMULA_EDITOR:
        if (lfoEditSwitch)
        {
            lfoEditSwitch->setValue(0.0);
            lfoEditSwitch->asJuceComponent()->repaint();
        }
        break;
    default:
        break;
    }

    if (isAnyOverlayPresent(olt))
    {
        dismissEditorOfType(olt);
    }

    switch (olt)
    {
    case MODULATION_EDITOR:
        frame->repaint();
        break;
    default:
        break;
    }
}

void SurgeGUIEditor::addJuceEditorOverlay(
    std::unique_ptr<juce::Component> c,
    std::string editorTitle, // A window display title - whatever you want
    OverlayTags editorTag,   // A tag by editor class. Please unique, no spaces.
    const juce::Rectangle<int> &containerSize, bool showCloseButton, std::function<void()> onClose)
{
    auto ol = std::make_unique<Surge::Overlays::OverlayWrapper>();
    ol->setBounds(containerSize);
    ol->setTitle(editorTitle);
    ol->setSkin(currentSkin, bitmapStore);
    ol->setSurgeGUIEditor(this);
    ol->setIcon(bitmapStore->getImage(IDB_SURGE_ICON));
    ol->setShowCloseButton(showCloseButton);
    ol->setCloseOverlay([this, editorTag, onClose]() {
        this->dismissEditorOfType(editorTag);
        onClose();
    });

    ol->addAndTakeOwnership(std::move(c));
    frame->addAndMakeVisible(*ol);
    juceOverlays[editorTag] = std::move(ol);
}

juce::Component *SurgeGUIEditor::getOverlayIfOpen(OverlayTags tag)
{
    if (juceOverlays.find(tag) == juceOverlays.end())
        return nullptr;

    if (!juceOverlays[tag])
        return nullptr;

    return juceOverlays[tag]->primaryChild.get();
}

void SurgeGUIEditor::updateWaveshaperOverlay()
{
    auto wso =
        dynamic_cast<Surge::Overlays::WaveShaperAnalysis *>(getOverlayIfOpen(WAVESHAPER_ANALYZER));
    auto t = synth->storage.getPatch().scene[current_scene].wsunit.type.val.i;
    if (wso)
    {
        wso->setWSType(t);
        wso->repaint();
    }
}