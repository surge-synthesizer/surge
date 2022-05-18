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
#include "overlays/FilterAnalysis.h"
#include "overlays/OverlayWrapper.h"
#include "overlays/KeyBindingsOverlay.h"
#include "widgets/MainFrame.h"
#include "widgets/WaveShaperSelector.h"
#include "UserDefaults.h"

std::unique_ptr<Surge::Overlays::OverlayComponent> SurgeGUIEditor::makeStorePatchDialog()
{
    auto npc = Surge::Skin::Connector::NonParameterConnection::SAVE_PATCH_DIALOG;
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
    bool appendOGPatchBy = Surge::Storage::getUserDefaultValue(
        &(this->synth->storage), Surge::Storage::AppendOriginalPatchBy, true);
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

    if (oldAuthor != "" && appendOGPatchBy)
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
    pb->setStorage(&(this->synth->storage));
    pb->setStoreTuningInPatch(this->synth->storage.getPatch().patchTuning.tuningStoredInPatch);

    // since it is now modal center in the window
    auto posRect = skinCtrl->getRect().withCentre(frame->getBounds().getCentre());
    pb->setEnclosingParentTitle("Save Patch");
    pb->setEnclosingParentPosition(posRect);
    pb->setHasIndependentClose(false);
    return pb;
}

std::unique_ptr<Surge::Overlays::OverlayComponent> SurgeGUIEditor::createOverlay(OverlayTags olt)
{
    auto locationGet = [this](Surge::Overlays::OverlayComponent *oc,
                              Surge::Skin::Connector::NonParameterConnection npc,
                              Surge::Storage::DefaultKey defKey) {
        auto conn = Surge::Skin::Connector::connectorByNonParameterConnection(npc);
        auto skinCtrl = currentSkin->getOrCreateControlForConnector(conn);
        auto dl = skinCtrl->getRect().getTopLeft();

        int sentinel = -1000004;
        auto ploc = Surge::Storage::getUserDefaultValue(&(synth->storage), defKey,
                                                        std::make_pair(sentinel, sentinel));

        if (ploc.first != sentinel && ploc.second != sentinel)
        {
            auto px = ploc.first;
            auto py = ploc.second;
            oc->setEnclosingParentPosition(juce::Rectangle<int>(
                px, py, skinCtrl->getRect().getWidth(), skinCtrl->getRect().getHeight()));
        }
        else
        {
            oc->setEnclosingParentPosition(skinCtrl->getRect());
        }

        oc->defaultLocation = dl;
        oc->setCanMoveAround(std::make_pair(true, defKey));
    };

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

        jassert(false); // Make a key for me please!

        pt->setCanTearOut({true, Surge::Storage::nKeys});

        return pt;
    }
    break;

    case MSEG_EDITOR:
    {
        auto lfo_id = modsource_editor[current_scene] - ms_lfo1;

        msegIsOpenFor = lfo_id;
        msegIsOpenInScene = current_scene;

        auto lfodata = &synth->storage.getPatch().scene[current_scene].lfo[lfo_id];

        if (lfodata->shape.val.i != lt_mseg)
        {
            lfodata = nullptr;

            // This can happen if restoring a torn out MSEG when the current LFO is selected
            // to a non-MSEG in open/close - in that case just find the first MSEG
            for (int sc = 0; sc < n_scenes && !lfodata; ++sc)
            {
                for (int lf = 0; lf < n_lfos && !lfodata; ++lf)
                {
                    if (synth->storage.getPatch().scene[sc].lfo[lf].shape.val.i == lt_mseg)
                    {
                        lfodata = &(synth->storage.getPatch().scene[sc].lfo[lf]);
                    }
                }
            }
        }

        if (!lfodata)
        {
            return nullptr;
        }

        auto ms = &synth->storage.getPatch().msegs[current_scene][lfo_id];
        auto mse = std::make_unique<Surge::Overlays::MSEGEditor>(
            &(synth->storage), lfodata, ms, &msegEditState[current_scene][lfo_id], currentSkin,
            bitmapStore, this);

        mse->onModelChanged = [this]() {
            if (lfoDisplayRepaintCountdown == 0)
            {
                lfoDisplayRepaintCountdown = 2;
            }
        };

        std::string title = modsource_names[modsource_editor[current_scene]];
        title += " Editor";
        Surge::Storage::findReplaceSubstring(title, std::string("LFO"), std::string("MSEG"));

        mse->setEnclosingParentTitle(title);
        mse->setCanTearOut({true, Surge::Storage::MSEGOverlayLocationTearOut});
        mse->setCanTearOutResize({true, Surge::Storage::MSEGOverlaySizeTearOut});
        mse->setMinimumSize(600, 250);
        locationGet(mse.get(), Surge::Skin::Connector::NonParameterConnection::MSEG_EDITOR_WINDOW,
                    Surge::Storage::MSEGOverlayLocation);

        return mse;
    }

    case FORMULA_EDITOR:
    {
        auto lfo_id = modsource_editor[current_scene] - ms_lfo1;
        auto fs = &synth->storage.getPatch().formulamods[current_scene][lfo_id];
        auto lfodata = &synth->storage.getPatch().scene[current_scene].lfo[lfo_id];

        if (lfodata->shape.val.i != lt_formula)
        {
            lfodata = nullptr;

            // as above in MSEG_EDITOR case
            for (int sc = 0; sc < n_scenes && !lfodata; ++sc)
            {
                for (int lf = 0; lf < n_lfos && !lfodata; ++lf)
                {
                    if (synth->storage.getPatch().scene[sc].lfo[lf].shape.val.i == lt_formula)
                    {
                        fs = &synth->storage.getPatch().formulamods[sc][lf];
                        lfodata = &(synth->storage.getPatch().scene[sc].lfo[lf]);
                    }
                }
            }
        }

        if (!fs)
        {
            return nullptr;
        }

        auto fme = std::make_unique<Surge::Overlays::FormulaModulatorEditor>(
            this, &(this->synth->storage),
            &synth->storage.getPatch().scene[current_scene].lfo[lfo_id], fs, lfo_id, current_scene,
            currentSkin);

        std::string title = modsource_names[modsource_editor[current_scene]];
        title += " Editor";
        Surge::Storage::findReplaceSubstring(title, std::string("LFO"), std::string("Formula"));

        fme->setSkin(currentSkin, bitmapStore);
        fme->setEnclosingParentTitle(title);
        fme->setCanTearOut({true, Surge::Storage::FormulaOverlayLocationTearOut});
        fme->setCanTearOutResize({true, Surge::Storage::FormulaOverlaySizeTearOut});
        fme->setMinimumSize(500, 250);
        locationGet(fme.get(),
                    Surge::Skin::Connector::NonParameterConnection::FORMULA_EDITOR_WINDOW,
                    Surge::Storage::FormulaOverlayLocation);

        return fme;
    }

    case SAVE_PATCH:
    {
        return makeStorePatchDialog();
    }

    case TUNING_EDITOR:
    {
        auto te = std::make_unique<Surge::Overlays::TuningOverlay>();

        te->setStorage(&(this->synth->storage));
        te->setEditor(this);
        te->setSkin(currentSkin, bitmapStore);
        te->setTuning(synth->storage.currentTuning);
        te->setEnclosingParentTitle("Tuning Editor");
        te->setCanTearOut({true, Surge::Storage::TuningOverlayLocationTearOut});
        te->setCanTearOutResize({true, Surge::Storage::TuningOverlaySizeTearOut});
        te->setMinimumSize(730, 400);
        locationGet(te.get(), Surge::Skin::Connector::NonParameterConnection::TUNING_EDITOR_WINDOW,
                    Surge::Storage::TuningOverlayLocation);

        return te;
    }

    case WT_SCRIPTING_EDITOR:
    {
        int w = 800, h = 520;
        auto px = (getWindowSizeX() - w) / 2;
        auto py = (getWindowSizeY() - h) / 2;
        auto r = juce::Rectangle<int>(px, py, w, h);

        auto os = &synth->storage.getPatch().scene[current_scene].osc[current_osc[current_scene]];

        auto wtse = std::make_unique<Surge::Overlays::WavetableEquationEditor>(
            this, &(this->synth->storage), os, currentSkin);

        wtse->setSkin(currentSkin, bitmapStore);
        wtse->setEnclosingParentPosition(juce::Rectangle<int>(px, py, w, h));
        wtse->setEnclosingParentTitle("Wavetable Script Editor");

        return wtse;
    }

    case WAVESHAPER_ANALYZER:
    {
        auto wsa =
            std::make_unique<Surge::Overlays::WaveShaperAnalysis>(this, &(this->synth->storage));

        locationGet(wsa.get(),
                    Surge::Skin::Connector::NonParameterConnection::WAVESHAPER_ANALYSIS_WINDOW,
                    Surge::Storage::WSAnalysisOverlayLocation);

        wsa->setSkin(currentSkin, bitmapStore);
        wsa->setEnclosingParentTitle("Waveshaper Analysis");
        wsa->setWSType(synth->storage.getPatch().scene[current_scene].wsunit.type.val.i);

        return wsa;
    }

    case FILTER_ANALYZER:
    {
        auto fa = std::make_unique<Surge::Overlays::FilterAnalysis>(this, &(this->synth->storage));

        locationGet(fa.get(),
                    Surge::Skin::Connector::NonParameterConnection::FILTER_ANALYSIS_WINDOW,
                    Surge::Storage::FilterAnalysisOverlayLocation);

        fa->setSkin(currentSkin, bitmapStore);
        fa->setEnclosingParentTitle("Filter Analysis");
        fa->setCanTearOut({true, Surge::Storage::FilterAnalysisOverlayLocationTearOut});
        fa->setCanTearOutResize({true, Surge::Storage::FilterAnalysisOverlaySizeTearOut});
        fa->setMinimumSize(300, 200);

        return fa;
    }

    case MODULATION_EDITOR:
    {
        auto me = std::make_unique<Surge::Overlays::ModulationEditor>(this, this->synth);
        me->setEnclosingParentTitle("Modulation List");
        me->setCanTearOut({true, Surge::Storage::ModlistOverlayLocationTearOut});
        me->setCanTearOutResize({true, Surge::Storage::ModlistOverlaySizeTearOut});
        me->setMinimumSize(600, 300);
        me->setSkin(currentSkin, bitmapStore);
        locationGet(me.get(), Surge::Skin::Connector::NonParameterConnection::MOD_LIST_WINDOW,
                    Surge::Storage::ModlistOverlayLocation);

        return me;
    }

    case KEYBINDINGS_EDITOR:
    {
        auto kb = std::make_unique<Surge::Overlays::KeyBindingsOverlay>(&(synth->storage), this);
        auto posRect =
            juce::Rectangle<int>(0, 0, 500, 500).withCentre(frame->getBounds().getCentre());

        kb->setSkin(currentSkin, bitmapStore);
        kb->setEnclosingParentPosition(posRect);
        kb->setEnclosingParentTitle("Keyboard Shortcut Editor");

        return kb;
    }

    // TODO: Implement the action history overlay!
    case ACTION_HISTORY:
    {
        return nullptr;
    }

    default:
        break;
    }

    return nullptr;
}
void SurgeGUIEditor::showOverlay(OverlayTags olt,
                                 std::function<void(Surge::Overlays::OverlayComponent *)> setup)
{
    auto ol = createOverlay(olt);

    if (!ol)
    {
        juceOverlays.erase(olt);
        return;
    }

    // copy these before the std::move below
    auto t = ol->getEnclosingParentTitle();
    auto r = ol->getEnclosingParentPosition();
    auto c = ol->getHasIndependentClose();

    setup(ol.get());

    std::function<void()> onClose = []() {};
    bool isModal = false;

    switch (olt)
    {
    case MSEG_EDITOR:
        onClose = [this]() {
            broadcastMSEGState();
            if (lfoEditSwitch)
            {
                lfoEditSwitch->setValue(0.0);
                lfoEditSwitch->asJuceComponent()->repaint();
            }
        };
        break;
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
    case FILTER_ANALYZER:
    case WAVESHAPER_ANALYZER:
        onClose = [this]() { this->synth->refresh_editor = true; };
        break;
    case SAVE_PATCH:
        isModal = true;
        break;
    case KEYBINDINGS_EDITOR:
        isModal = true;
        break;
    default:
        break;
    }
    addJuceEditorOverlay(std::move(ol), t, olt, r, c, onClose, isModal);

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

    if (isTornOut.find(olt) != isTornOut.end())
    {
        if (isTornOut[olt])
        {
            getOverlayWrapperIfOpen(olt)->doTearOut();
        }
    }

    getOverlayIfOpen(olt)->grabKeyboardFocus();
}
void SurgeGUIEditor::closeOverlay(OverlayTags olt)
{
    auto olw = getOverlayWrapperIfOpen(olt);
    if (olw)
    {
        isTornOut[olt] = olw->isTornOut();
    }
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

Surge::Overlays::OverlayWrapper *SurgeGUIEditor::addJuceEditorOverlay(
    std::unique_ptr<juce::Component> c,
    std::string editorTitle, // A window display title - whatever you want
    OverlayTags editorTag,   // A tag by editor class. Please unique, no spaces.
    const juce::Rectangle<int> &containerSize, bool showCloseButton, std::function<void()> onClose,
    bool forceModal)
{
    std::unique_ptr<Surge::Overlays::OverlayWrapper> ol;

    if (forceModal)
    {
        ol = std::make_unique<Surge::Overlays::OverlayWrapper>(containerSize);
        ol->setBounds(frame->getBounds());
    }
    else
    {
        ol = std::make_unique<Surge::Overlays::OverlayWrapper>();
        ol->setBounds(containerSize);
    }
    ol->setWindowTitle(editorTitle);
    ol->setSkin(currentSkin, bitmapStore);
    ol->setSurgeGUIEditor(this);
    ol->setStorage(&(this->synth->storage));
    ol->setIcon(bitmapStore->getImage(IDB_SURGE_ICON));
    ol->setShowCloseButton(showCloseButton);
    ol->setCloseOverlay([this, editorTag, onClose]() {
        this->dismissEditorOfType(editorTag);
        onClose();
    });

    auto olc = dynamic_cast<Surge::Overlays::OverlayComponent *>(c.get());
    if (olc)
    {
        ol->setCanTearOut(olc->getCanTearOut());
        ol->setCanTearOutResize(olc->getCanTearOutResize());
    }

    ol->addAndTakeOwnership(std::move(c));
    addAndMakeVisibleWithTracking(frame.get(), *ol);

    juceOverlays[editorTag] = std::move(ol);
    if (overlayConsumesKeyboard(editorTag))
        vkbForward++;
    return juceOverlays[editorTag].get();
}

void SurgeGUIEditor::dismissEditorOfType(OverlayTags ofType)
{
    if (juceOverlays.empty())
        return;

    if (overlayConsumesKeyboard(ofType))
        vkbForward--;

    auto ow = getOverlayWrapperIfOpen(ofType);
    if (ow)
        isTornOut[ofType] = ow->isTornOut();

    if (juceOverlays.find(ofType) != juceOverlays.end())
    {
        if (juceOverlays[ofType])
        {
            frame->removeChildComponent(juceOverlays[ofType].get());
            juceDeleteOnIdle.push_back(std::move(juceOverlays[ofType]));
        }
        juceOverlays.erase(ofType);
    }
}

bool SurgeGUIEditor::isPointWithinOverlay(const juce::Point<int> point)
{
    for (int i = 0; i < SurgeGUIEditor::n_overlay_tags; i++)
    {
        auto tag = static_cast<SurgeGUIEditor::OverlayTags>(i);
        auto olw = getOverlayWrapperIfOpen(tag);

        if (olw && !olw->isTornOut())
        {
            juce::Rectangle<int> olrect = olw->getBounds();

            if (olrect.contains(point))
            {
                return true;
            }
        }
    }

    return false;
}

bool SurgeGUIEditor::overlayConsumesKeyboard(OverlayTags ofType)
{
    switch (ofType)
    {
    case PATCH_BROWSER:
    case SAVE_PATCH:
    case FORMULA_EDITOR:
        return true;
    default:
        break;
    }
    return false;
}

juce::Component *SurgeGUIEditor::getOverlayIfOpen(OverlayTags tag)
{
    if (juceOverlays.find(tag) == juceOverlays.end())
        return nullptr;

    if (!juceOverlays[tag])
        return nullptr;

    return juceOverlays[tag]->primaryChild.get();
}

Surge::Overlays::OverlayWrapper *SurgeGUIEditor::getOverlayWrapperIfOpen(OverlayTags tag)
{
    if (juceOverlays.find(tag) == juceOverlays.end())
        return nullptr;

    if (!juceOverlays[tag])
        return nullptr;

    return dynamic_cast<Surge::Overlays::OverlayWrapper *>(juceOverlays[tag].get());
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

void SurgeGUIEditor::rezoomOverlays()
{
    for (const auto &p : juceOverlays)
    {
        if (p.second->isTornOut())
        {
            auto c = p.second->primaryChild.get();
            auto w = c->getWidth();
            auto h = c->getHeight();

            c->getTransform().inverted().transformPoint(w, h);
            c->setTransform(juce::AffineTransform::scale(zoomFactor / 100.0));
            c->getTransform().transformPoint(w, h);

            c->setSize(w, h);
            p.second->tearOutParent->setContentComponentSize(w, h);
        }
    }
}

void SurgeGUIEditor::frontNonModalOverlays()
{
    std::vector<juce::Component *> frontthese;

    for (auto c : frame->getChildren())
    {
        if (auto ol = dynamic_cast<Surge::Overlays::OverlayWrapper *>(c))
        {
            if (!ol->getIsModal())
            {
                frontthese.push_back(c);
            }
        }
    }

    for (auto f : frontthese)
    {
        f->toFront(true);
    }
}

void SurgeGUIEditor::refreshAndMorphOverlayWithOpenClose(OverlayTags tag, OverlayTags newTag)
{
    if (!isAnyOverlayPresent(tag))
        return;

    bool to{false};
    juce::Point<int> tol{-1, -1};
    auto olw = getOverlayWrapperIfOpen(tag);
    if (olw)
    {
        to = olw->isTornOut();
        tol = olw->currentTearOutLocation();
    }
    /*
     * Some editors can do better than a forced open-clsoe
     */
    auto couldRefresh = updateOverlayContentIfPresent(newTag);
    if (!couldRefresh)
    {
        closeOverlay(tag);
        showOverlay(newTag);
        if (to)
        {
            olw = getOverlayWrapperIfOpen(newTag);
            if (olw)
                olw->doTearOut(tol);
        }
    }
}

bool SurgeGUIEditor::updateOverlayContentIfPresent(OverlayTags tag)
{
    auto olw = getOverlayWrapperIfOpen(tag);
    bool couldRefresh = true;
    switch (tag)
    {
    case TUNING_EDITOR:
    {
        auto tunol = dynamic_cast<Surge::Overlays::TuningOverlay *>(getOverlayIfOpen(tag));
        if (tunol)
        {
            tunol->setTuning(synth->storage.currentTuning);
        }
        break;
    }
    case MODULATION_EDITOR:
    {
        auto modol = dynamic_cast<Surge::Overlays::ModulationEditor *>(getOverlayIfOpen(tag));

        if (modol)
        {
            modol->rebuildContents();
        }
        break;
    }
    case WAVESHAPER_ANALYZER:
    {
        updateWaveshaperOverlay();
        break;
    }
    case FILTER_ANALYZER:
    {
        auto f = getOverlayIfOpen(tag);
        if (f)
            f->repaint();
        break;
    }
    default:
        couldRefresh = false;
        break;
    }
    return couldRefresh;
}
