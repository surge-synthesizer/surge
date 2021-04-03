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
#include "COscillatorDisplay.h"
#include "Oscillator.h"
#include <time.h>
#include "unitconversion.h"
#include "UserInteractions.h"
#include "guihelpers.h"
#include "SkinColors.h"
#include "RuntimeFont.h"

#include "filesystem/import.h"
#include "guihelpers.h"
#include "CursorControlGuard.h"
#include <utility>

/*
 * Custom Editor Types
 */
#include "AliasOscillator.h"

using namespace VSTGUI;

const float disp_pitch = 90.15f - 48.f;
const int wtbheight = 12;

extern CFontRef displayFont;

void COscillatorDisplay::draw(CDrawContext *dc)
{
    if (customEditor && !canHaveCustomEditor())
    {
        closeCustomEditor();
    }

    if (customEditor && customEditorActive)
    {
        customEditor->draw(dc);
        drawExtraEditButton(dc, "CLOSE");
        return;
    }

    pdata tp[2][n_scene_params]; // 0 is orange, 1 is blue
    Oscillator *osces[2];
    std::string olabel;
    for (int c = 0; c < 2; ++c)
    {
        osces[c] = nullptr;

#if OSC_MOD_ANIMATION
        if (!is_mod && c > 0)
            continue;
#else
        if (c > 0)
            continue;
#endif

        tp[c][oscdata->pitch.param_id_in_scene].f = 0;
        for (int i = 0; i < n_osc_params; i++)
            tp[c][oscdata->p[i].param_id_in_scene].i = oscdata->p[i].val.i;

#if OSC_MOD_ANIMATION
        if (c == 1)
        {
            auto modOut = 1.0;
            auto activeScene = storage->getPatch().scene_active.val.i;
            auto *scene = &(storage->getPatch().scene[activeScene]);
            auto iter = scene->modulation_voice.begin();

            // You'd think you could use this right? But you can't. These are NULL if you don't have
            // a voice except scene ones ModulationSource *ms = scene->modsources[modsource];

            bool isUnipolar = true;
            double smt = 1.0 - (mod_time - (int)mod_time); // 1hz ramp

            std::ostringstream oss;
            oss << modsource_names[modsource];
            olabel = oss.str();

            if (modsource >= ms_lfo1 && modsource <= ms_slfo6)
            {
                auto *lfo = &(scene->lfo[modsource - ms_lfo1]);

                float frate = lfo->rate.val.f;
                if (lfo->rate.temposync)
                    frate *= storage->temposyncratio;

                auto freq = powf(2.0f, frate);

                smt = sin(M_PI * freq * mod_time);
                if (lfo->shape.val.i == lt_square)
                    smt = (smt > 0 ? 1 : -1);

                if (lfo->unipolar.val.i)
                {
                    smt = 0.5 * (smt + 1);
                }
            }

            while (iter != scene->modulation_voice.end())
            {
                int src_id = iter->source_id;
                if (src_id == modsource)
                {
                    int dst_id = iter->destination_id;
                    float depth = iter->depth;
                    tp[c][dst_id].f += depth * modOut * smt;
                }
                iter++;
            }

            iter = scene->modulation_scene.begin();

            while (iter != scene->modulation_scene.end())
            {
                int src_id = iter->source_id;
                if (src_id == modsource)
                {
                    int dst_id = iter->destination_id;
                    float depth = iter->depth;
                    tp[c][dst_id].f += depth * modOut * smt;
                }
                iter++;
            }
        }
#endif

        osces[c] = spawn_osc(oscdata->type.val.i, storage, oscdata, tp[c]);
    }

    float h = getHeight();
    float extraYTranslate = 0;
    if (uses_wavetabledata(oscdata->type.val.i))
    {
        h -= wtbheight + 2.5;
        extraYTranslate = 0.1 * wtbheight;
    }

    int totalSamples = (1 << 4) * (int)getWidth();
    int averagingWindow = 4; // this must be both less than BLOCK_SIZE_OS and BLOCK_SIZE_OS must be
                             // an integer multiple of it

#if LINUX && !TARGET_JUCE_UI
    Surge::UI::NonIntegralAntiAliasGuard naag(dc);
#endif

    float valScale = 100.0f;

    auto size = getViewSize();

    for (int c = 1; c >= 0; --c) // backwards so we draw blue first
    {
        bool use_display = false;
        Oscillator *osc = osces[c];
        CGraphicsPath *path = dc->createGraphicsPath();
        if (osc)
        {
            float disp_pitch_rs = disp_pitch + 12.0 * log2(dsamplerate / 44100.0);
            if (!storage->isStandardTuning)
            {
                // OK so in this case we need to find a better version of the note which gets us
                // that pitch. Sigh.
                auto pit = storage->note_to_pitch_ignoring_tuning(disp_pitch_rs);
                int bracket = -1;
                for (int i = 0; i < 128; ++i)
                {
                    if (storage->note_to_pitch(i) < pit && storage->note_to_pitch(i + 1) > pit)
                    {
                        bracket = i;
                        break;
                    }
                }
                if (bracket >= 0)
                {
                    float f1 = storage->note_to_pitch(bracket);
                    float f2 = storage->note_to_pitch(bracket + 1);
                    float frac = (pit - f1) / (f2 - f1);
                    disp_pitch_rs = bracket + frac;
                }
                else
                {
                    // What the hell type of scale is this folks! But this is just UI code so just
                    // punt
                }
            }
            use_display = osc->allow_display();

            // Mis-install check #2
            if (uses_wavetabledata(oscdata->type.val.i) && storage->wt_list.size() == 0)
                use_display = false;

            if (use_display)
                osc->init(disp_pitch_rs, true, true);

            int block_pos = BLOCK_SIZE_OS;
            for (int i = 0; i < totalSamples; i += averagingWindow)
            {
                if (use_display && (block_pos >= BLOCK_SIZE_OS))
                {
                    if (uses_wavetabledata(oscdata->type.val.i))
                    {
                        storage->waveTableDataMutex.lock();
                        osc->process_block(disp_pitch_rs);
                        block_pos = 0;
                        storage->waveTableDataMutex.unlock();
                    }
                    else
                    {
                        osc->process_block(disp_pitch_rs);
                        block_pos = 0;
                    }
                }

                float val = 0.f;
                if (use_display)
                {
                    for (int j = 0; j < averagingWindow; ++j)
                    {
                        val += osc->output[block_pos];
                        block_pos++;
                    }
                    val = val / averagingWindow;
                    val =
                        ((-val + 1.0f) * 0.5f * (1.0 - scaleDownBy) + 0.5 * scaleDownBy) * valScale;
                }
                block_pos++;
                float xc = valScale * i / totalSamples;

                // OK so val is now a value between 0 and valScale, and xc is a value between 0 and
                // valScale
                if (i == 0)
                {
                    path->beginSubpath(xc, val);
                }
                else
                {
                    path->addLine(xc, val);
                }
            }
            // srand( (unsigned)time( NULL ) );
        }
        // OK so now we need to figure out how to transfer the box with is [0,valscale] x
        // [0,valscale] to our coords. So scale then position
        VSTGUI::CGraphicsTransform tf = VSTGUI::CGraphicsTransform()
                                            .scale(getWidth() / valScale, h / valScale)
                                            .translate(size.getTopLeft().x, size.getTopLeft().y)
                                            .translate(0, extraYTranslate);
#if LINUX && !TARGET_JUCE_UI
        auto tmps = size;
        dc->getCurrentTransform().transform(tmps);
        VSTGUI::CGraphicsTransform tpath = VSTGUI::CGraphicsTransform()
                                               .scale(getWidth() / valScale, h / valScale)
                                               .translate(tmps.getTopLeft().x, tmps.getTopLeft().y);
#else
        auto tpath = tf;
#endif

        dc->saveGlobalState();

        CRect waveBoundsRect;
        if (c == 0)
        {
            float linesAreInBy = scaleDownBy * 0.5;
            // OK so draw the rules
            CPoint mid0(0.f, valScale / 2.f), mid1(valScale, valScale / 2.f);
            CPoint top0(0.f, valScale * (1.0 - linesAreInBy)),
                top1(valScale, valScale * (1.0 - linesAreInBy));
            CPoint bot0(0.f, valScale * linesAreInBy), bot1(valScale, valScale * linesAreInBy);

            CPoint clip0(0.f, valScale * linesAreInBy),
                clip1(valScale, valScale * (1.0 - linesAreInBy));
            tf.transform(mid0);

            tf.transform(mid1);
            tf.transform(top0);
            tf.transform(top1);
            tf.transform(bot0);
            tf.transform(bot1);
            tf.transform(clip0);
            tf.transform(clip1);

            waveBoundsRect =
                CRect(clip0.x, clip0.y - 0.5, clip1.x, clip1.y + 0.5); // allow draws on the line
            dc->setDrawMode(VSTGUI::kAntiAliasing | VSTGUI::kNonIntegralMode);

            dc->setLineWidth(1.0);
            dc->setFrameColor(skin->getColor(Colors::Osc::Display::Center));
            dc->drawLine(mid0, mid1);

            dc->setLineWidth(1.0);
            dc->setFrameColor(skin->getColor(Colors::Osc::Display::Bounds));
            dc->drawLine(top0, top1);
            dc->drawLine(bot0, bot1);

            // Now draw dots
            auto pointColor = skin->getColor(Colors::Osc::Display::Dots);
            int nxd = 21, nyd = 13;
            for (int xd = 0; xd < nxd; xd++)
            {
                float normx = 1.f * xd / (nxd - 1) * 0.99;
                for (int yd = 1; yd < nyd - 1; yd++)
                {
                    if (yd == (nyd - 1) / 2)
                        continue;

                    float normy = 1.f * yd / (nyd - 1);
                    auto dotPoint = CPoint(normx * valScale, (0.8 * normy + 0.1) * valScale);
                    tf.transform(dotPoint);
                    float esize = 0.5;
                    float xoff = (xd == 0 ? esize : 0);
                    auto er = CRect(dotPoint.x - esize + xoff, dotPoint.y - esize,
                                    dotPoint.x + esize + xoff, dotPoint.y + esize);
#if LINUX && !TARGET_JUCE_UI
                    dc->drawPoint(dotPoint, pointColor);
#else
                    dc->setFillColor(pointColor);
                    dc->drawEllipse(er, VSTGUI::kDrawFilled);
#endif
                }
            }

            // OK so now the label
            if (osces[1])
            {
                dc->setFontColor(skin->getColor(Colors::Osc::Display::AnimatedWave));
                dc->setFont(displayFont);
                CPoint lab0(0, valScale * 0.1 - 10);
                tf.transform(lab0);
                CRect rlab(lab0, CPoint(10, 10));
                dc->drawString(olabel.c_str(), rlab, kLeftText, true);
            }
        }

        dc->setLineWidth(1.3);
#if LINUX && !TARGET_JUCE_UI
        dc->setDrawMode(VSTGUI::kAntiAliasing | VSTGUI::kNonIntegralMode);
#else
        dc->setDrawMode(VSTGUI::kAntiAliasing);
#endif
        if (c == 1)
            dc->setFrameColor(VSTGUI::CColor(100, 100, 180, 0xFF));
        else
            dc->setFrameColor(skin->getColor(Colors::Osc::Display::Wave));

        if (use_display)
        {
            CRect oldcr;
#if TARGET_JUCE_UI
            dc->setClipRect(waveBoundsRect);
#endif
            dc->drawGraphicsPath(path, VSTGUI::CDrawContext::PathDrawMode::kPathStroked, &tpath);
        }
        dc->restoreGlobalState();

        path->forget();

        delete osces[c];
    }

    if (uses_wavetabledata(oscdata->type.val.i))
    {
        CRect wtlbl(size);
        wtlbl.top = wtlbl.bottom - wtbheight;
        wtlbl.offset(0, -4);
        rmenu = wtlbl;
        rmenu.inset(14, 0);

        rprev = wtlbl;
        rprev.right = rmenu.left; // -1;
        rnext = wtlbl;
        rnext.left = rmenu.right; // +1;

        char wttxt[256];

        storage->waveTableDataMutex.lock();

        int wtid = oscdata->wt.current_id;
        if (oscdata->wavetable_display_name[0] != '\0')
        {
            strcpy(wttxt, oscdata->wavetable_display_name);
        }
        else if ((wtid >= 0) && (wtid < storage->wt_list.size()))
        {
            strcpy(wttxt, storage->wt_list.at(wtid).name.c_str());
        }
        else if (oscdata->wt.flags & wtf_is_sample)
        {
            strcpy(wttxt, "(Patch Sample)");
        }
        else
        {
            strcpy(wttxt, "(Patch Wavetable)");
        }

        storage->waveTableDataMutex.unlock();

        char *r = strrchr(wttxt, '.');
        if (r)
            *r = 0;
        auto fgcol = skin->getColor(Colors::Osc::Filename::Background);
        auto fgframe = skin->getColor(Colors::Osc::Filename::Frame);
        auto fgtext = skin->getColor(Colors::Osc::Filename::Text);

        auto fgcolHov = skin->getColor(Colors::Osc::Filename::BackgroundHover);
        auto fgframeHov = skin->getColor(Colors::Osc::Filename::FrameHover);
        auto fgtextHov = skin->getColor(Colors::Osc::Filename::TextHover);

        dc->setFillColor(fgcol);
        auto rbg = rmenu;

        if (skin->getVersion() >= 2)
        {
            if (isWTHover == MENU)
            {
                dc->setFrameColor(fgframeHov);
                dc->setFillColor(fgcolHov);
            }
            else
            {
                dc->setFillColor(fgcol);
                dc->setFrameColor(fgframe);
            }
            dc->drawRect(rmenu, kDrawFilledAndStroked);
        }
        else
        {
            dc->setFillColor(fgcol);
            dc->drawRect(rmenu, kDrawFilled);
        }

        if (skin->getVersion() >= 2 && isWTHover == MENU)
        {
            dc->setFontColor(skin->getColor(Colors::Osc::Filename::TextHover));
        }
        else
        {
            dc->setFontColor(skin->getColor(Colors::Osc::Filename::Text));
        }
        dc->setFont(displayFont);
        dc->drawString(wttxt, rmenu, kCenterText, true);

        /*CRect wtlbl_status(size);
        wtlbl_status.bottom = wtlbl_status.top + wtbheight;
        dc->setFontColor(kBlackCColor);
        if(oscdata->wt.flags & wtf_is_sample) dc->drawString("IS
        SAMPLE",wtlbl_status,false,kRightText);*/

        if (skin->getVersion() == 1)
        {
            dc->setFillColor(fgcol);
            dc->drawRect(rprev, kDrawFilled);
            dc->drawRect(rnext, kDrawFilled);
        }
        else
        {
            if (isWTHover == PREV)
            {
                dc->setFrameColor(fgframeHov);
                dc->setFillColor(fgcolHov);
            }
            else
            {
                dc->setFillColor(fgcol);
                dc->setFrameColor(fgframe);
            }
            dc->drawRect(rprev, kDrawFilledAndStroked);

            if (isWTHover == NEXT)
            {
                dc->setFrameColor(fgframeHov);
                dc->setFillColor(fgcolHov);
            }
            else
            {
                dc->setFillColor(fgcol);
                dc->setFrameColor(fgframe);
            }
            dc->drawRect(rnext, kDrawFilledAndStroked);
        }
        dc->setFrameColor(kBlackCColor);

        dc->saveGlobalState();

        dc->setDrawMode(kAntiAliasing);

        auto marginy = 2;
        float triw = 6;
        float trih = rprev.getHeight() - (marginy * 2);
        float trianch = rprev.top + marginy;
        float triprevstart = rprev.left + ((rprev.getWidth() - triw) / 2.f);
        float trinextstart = rnext.right - ((rnext.getWidth() - triw) / 2.f);

        VSTGUI::CDrawContext::PointList trinext;
        if (skin->getVersion() >= 2 && isWTHover == NEXT)
        {
            dc->setFillColor(skin->getColor(Colors::Osc::Filename::TextHover));
        }
        else
        {
            dc->setFillColor(skin->getColor(Colors::Osc::Filename::Text));
        }
        trinext.push_back(VSTGUI::CPoint(trinextstart - triw, trianch));
        trinext.push_back(VSTGUI::CPoint(trinextstart, trianch + (trih / 2.f)));
        trinext.push_back(VSTGUI::CPoint(trinextstart - triw, trianch + trih));
        dc->drawPolygon(trinext, kDrawFilled);

        VSTGUI::CDrawContext::PointList triprev;
        if (skin->getVersion() >= 2 && isWTHover == PREV)
        {
            dc->setFillColor(skin->getColor(Colors::Osc::Filename::TextHover));
        }
        else
        {
            dc->setFillColor(skin->getColor(Colors::Osc::Filename::Text));
        }
        triprev.push_back(VSTGUI::CPoint(triprevstart + triw, trianch));
        triprev.push_back(VSTGUI::CPoint(triprevstart, trianch + (trih / 2.f)));
        triprev.push_back(VSTGUI::CPoint(triprevstart + triw, trianch + trih));

        dc->drawPolygon(triprev, kDrawFilled);

        dc->restoreGlobalState();
    }

    if (canHaveCustomEditor())
    {
        drawExtraEditButton(dc, "EDIT");
    }

    setDirty(false);
}

CMouseEventResult COscillatorDisplay::onMouseDown(CPoint &where, const CButtonState &button)
{
    if (canHaveCustomEditor() && customEditButtonRect.pointInside(where))
    {
        if (customEditorActive)
        {
            closeCustomEditor();
        }
        else
        {
            openCustomEditor();
        }
        invalid();
        return kMouseDownEventHandledButDontNeedMovedOrUpEvents;
    }

    if (customEditorActive && customEditor)
    {
        return customEditor->onMouseDown(where, button);
    }

    if (listener && (button & (kMButton | kButton4 | kButton5)))
    {
        listener->controlModifierClicked(this, button);
        return kMouseDownEventHandledButDontNeedMovedOrUpEvents;
    }

    assert(oscdata);

    if (!uses_wavetabledata(oscdata->type.val.i) || (where.y < rmenu.top))
    {
        controlstate = 1;
        lastpos.x = where.x;
        return kMouseEventHandled;
    }
    else if (uses_wavetabledata(oscdata->type.val.i))
    {
        int id = oscdata->wt.current_id;
        if (rprev.pointInside(where))
        {
            id = storage->getAdjacentWaveTable(oscdata->wt.current_id, false);
            if (id >= 0)
                oscdata->wt.queue_id = id;
        }
        else if (rnext.pointInside(where))
        {
            id = storage->getAdjacentWaveTable(oscdata->wt.current_id, true);
            if (id >= 0)
                oscdata->wt.queue_id = id;
        }
        else if (rmenu.pointInside(where))
        {
            CRect menurect(0, 0, 0, 0);
            menurect.offset(where.x, where.y);
            COptionMenu *contextMenu =
                new COptionMenu(menurect, 0, 0, 0, 0, COptionMenu::kMultipleCheckStyle);

            populateMenu(contextMenu, id);

            getFrame()->addView(contextMenu); // add to frame
            contextMenu->setDirty();
            contextMenu->popup();
#if !TARGET_JUCE_UI
            // wth is this?
            contextMenu->onMouseDown(where, kLButton); // <-- modal menu loop is here
#endif
            // getFrame()->looseFocus(pContext);

            getFrame()->removeView(contextMenu, true); // remove from frame and forget
        }
    }
    return kMouseDownEventHandledButDontNeedMovedOrUpEvents;
}

void COscillatorDisplay::populateMenu(COptionMenu *contextMenu, int selectedItem)
{
    int idx = 0;
    bool needToAddSep = false;
    for (auto c : storage->wtCategoryOrdering)
    {
        if (idx == storage->firstThirdPartyWTCategory ||
            (idx == storage->firstUserWTCategory &&
             storage->firstUserWTCategory != storage->wt_category.size()))
        {
            needToAddSep = true; // only add if there's actually data coming so defer the add
        }

        idx++;

        PatchCategory cat = storage->wt_category[c];
        if (cat.numberOfPatchesInCategoryAndChildren == 0)
            continue;

        if (cat.isRoot)
        {
            if (needToAddSep)
            {
                contextMenu->addEntry("-");
                needToAddSep = false;
            }
            populateMenuForCategory(contextMenu, c, selectedItem);
        }
    }

    // Add direct open here
    contextMenu->addSeparator();

    auto renameItem = new CCommandMenuItem(
        CCommandMenuItem::Desc(Surge::UI::toOSCaseForMenu("Change Wavetable Display Name...")));
    auto rnaction = [this](CCommandMenuItem *item) {
        char c[256];
        strncpy(c, this->oscdata->wavetable_display_name, 256);
        auto *sge = dynamic_cast<SurgeGUIEditor *>(listener);
        if (sge)
        {
            sge->promptForMiniEdit(
                c, "Enter a custom wavetable display name:", "Wavetable Display Name",
                CPoint(-1, -1), [this](const std::string &s) {
                    strncpy(this->oscdata->wavetable_display_name, s.c_str(), 256);
                    this->invalid();
                });
        }
    };
    renameItem->setActions(rnaction, nullptr);
    contextMenu->addEntry(renameItem);

    auto refreshItem = new CCommandMenuItem(
        CCommandMenuItem::Desc(Surge::UI::toOSCaseForMenu("Refresh Wavetable List")));
    auto refresh = [this](CCommandMenuItem *item) { this->storage->refresh_wtlist(); };
    refreshItem->setActions(refresh, nullptr);
    contextMenu->addEntry(refreshItem);

    contextMenu->addSeparator();

    auto actionItem = new CCommandMenuItem(
        CCommandMenuItem::Desc(Surge::UI::toOSCaseForMenu("Load Wavetable from File...")));
    auto action = [this](CCommandMenuItem *item) { this->loadWavetableFromFile(); };
    actionItem->setActions(action, nullptr);
    contextMenu->addEntry(actionItem);

    auto exportItem = new CCommandMenuItem(
        CCommandMenuItem::Desc(Surge::UI::toOSCaseForMenu("Save Wavetable to File...")));
    auto exportAction = [this](CCommandMenuItem *item) {
        int oscNum = this->osc_in_scene;
        int scene = this->scene;

        std::string baseName = storage->getPatch().name + "_osc" + std::to_string(oscNum + 1) +
                               "_scene" + (scene == 0 ? "A" : "B");
        storage->export_wt_wav_portable(baseName, &(oscdata->wt));
    };
    exportItem->setActions(exportAction, nullptr);
    contextMenu->addEntry(exportItem);

    auto omi = new CCommandMenuItem(
        CCommandMenuItem::Desc(Surge::UI::toOSCaseForMenu("Open Exported Wavetables Folder...")));
    omi->setActions([this](CCommandMenuItem *i) {
        Surge::UserInteractions::openFolderInFileBrowser(
            Surge::Storage::appendDirectory(this->storage->userDataPath, "Exported Wavetables"));
    });
    contextMenu->addEntry(omi);

    auto *sge = dynamic_cast<SurgeGUIEditor *>(listener);
    if (sge)
    {
        auto hu = sge->helpURLForSpecial("wavetables");
        if (hu != "")
        {
            auto lurl = sge->fullyResolvedHelpURL(hu);
            auto hi = new CCommandMenuItem(CCommandMenuItem::Desc("[?] Wavetables"));
            auto ca = [lurl](CCommandMenuItem *i) { Surge::UserInteractions::openURL(lurl); };
            hi->setActions(ca, nullptr);
            contextMenu->addSeparator();
            contextMenu->addEntry(hi);
        }
    }
}

bool COscillatorDisplay::populateMenuForCategory(COptionMenu *contextMenu, int categoryId,
                                                 int selectedItem)
{
    char name[NAMECHARS];
    COptionMenu *subMenu =
        new COptionMenu(getViewSize(), 0, categoryId, 0, 0, COptionMenu::kMultipleCheckStyle);
    subMenu->setNbItemsPerColumn(32);
    int sub = 0;

    PatchCategory cat = storage->wt_category[categoryId];

    for (auto p : storage->wtOrdering)
    {
        if (storage->wt_list[p].category == categoryId)
        {
            sprintf(name, "%s", storage->wt_list[p].name.c_str());
            auto actionItem = new CCommandMenuItem(CCommandMenuItem::Desc(name));
            auto action = [this, p](CCommandMenuItem *item) { this->loadWavetable(p); };

            if (p == selectedItem)
                actionItem->setChecked(true);
            actionItem->setActions(action, nullptr);
            subMenu->addEntry(actionItem);

            sub++;
        }
    }

    bool selected = false;

    for (auto child : cat.children)
    {
        if (child.numberOfPatchesInCategoryAndChildren > 0)
        {
            // this isn't the best approach but it works
            int cidx = 0;
            for (auto &cc : storage->wt_category)
            {
                if (cc.name == child.name)
                    break;
                cidx++;
            }

            bool subSel = populateMenuForCategory(subMenu, cidx, selectedItem);
            selected = selected || subSel;
        }
    }

    if (!cat.isRoot)
    {
        std::string catName = storage->wt_category[categoryId].name;
        std::size_t sepPos = catName.find_last_of(PATH_SEPARATOR);
        if (sepPos != std::string::npos)
        {
            catName = catName.substr(sepPos + 1);
        }
        strncpy(name, catName.c_str(), NAMECHARS);
    }
    else
    {
        strncpy(name, storage->wt_category[categoryId].name.c_str(), NAMECHARS);
    }

    CMenuItem *submenuItem = contextMenu->addEntry(subMenu, name);

    if (selected || (selectedItem >= 0 && storage->wt_list[selectedItem].category == categoryId))
    {
        selected = true;
        submenuItem->setChecked(true);
    }

    subMenu->forget(); // Important, so that the refcounter gets right
    return selected;
}

void COscillatorDisplay::loadWavetable(int id)
{
    if (id >= 0 && (id < storage->wt_list.size()))
    {
        oscdata->wt.queue_id = id;
    }
}

void COscillatorDisplay::loadWavetableFromFile()
{
    Surge::UserInteractions::promptFileOpenDialog("", "", "", [this](std::string s) {
        strncpy(this->oscdata->wt.queue_filename, s.c_str(), 255);
    });
}

CMouseEventResult COscillatorDisplay::onMouseUp(CPoint &where, const CButtonState &buttons)
{
    if (customEditorActive && customEditor)
    {
        return customEditor->onMouseUp(where, buttons);
    }
    if (controlstate)
    {
        controlstate = 0;
    }
    return kMouseEventHandled;
}
CMouseEventResult COscillatorDisplay::onMouseMoved(CPoint &where, const CButtonState &buttons)
{
    bool newEditButton = false;

    if (canHaveCustomEditor() && customEditButtonRect.pointInside(where))
    {
        newEditButton = true;
    }

    if (newEditButton != editButtonHover)
    {
        editButtonHover = newEditButton;
        invalid();
    }

    if (customEditorActive && customEditor)
    {
        return customEditor->onMouseMoved(where, buttons);
    }

    if (uses_wavetabledata(oscdata->type.val.i))
    {
        auto owt = isWTHover;
        isWTHover = NONE;
        if (uses_wavetabledata(oscdata->type.val.i))
        {
            if (rprev.pointInside(where))
            {
                isWTHover = PREV;
            }
            else if (rnext.pointInside(where))
            {
                isWTHover = NEXT;
            }
            else if (rmenu.pointInside(where))
            {
                isWTHover = MENU;
            }
        }
        if (owt != isWTHover)
            invalid();
    }
    else
    {
        if (isWTHover != NONE)
        {
            isWTHover = NONE;
            invalid();
        }
        // getFrame()->setCursor( VSTGUI::kCursorDefault );
    }

    if (controlstate)
    {
        /*oscdata->startphase.val.f -= 0.005f * (where.x - lastpos.x);
        oscdata->startphase.val.f = limit_range(oscdata->startphase.val.f,0.f,1.f);
        lastpos.x = where.x;
        invalid();*/
    }
    return kMouseEventHandled;
}

bool COscillatorDisplay::onWheel(const VSTGUI::CPoint &where, const float &distance,
                                 const VSTGUI::CButtonState &buttons)
{
    if (customEditorActive && customEditor)
    {
        return customEditor->onWheel(where, distance, buttons);
    }
    return false;
}

void COscillatorDisplay::invalidateIfIdIsInRange(int id)
{
    auto *currOsc = &oscdata->type;
    auto *endOsc = &oscdata->retrigger;
    bool oscInvalid = false;
    while (currOsc <= endOsc && !oscInvalid)
    {
        if (currOsc->id == id)
            oscInvalid = true;
        currOsc++;
    }

    if (oscInvalid)
    {
        invalid();
    }
}
CMouseEventResult COscillatorDisplay::onMouseEntered(CPoint &where, const CButtonState &buttons)
{
    isWTHover = NONE;
    if (uses_wavetabledata(oscdata->type.val.i))
    {
        if (rprev.pointInside(where))
        {
            isWTHover = PREV;
        }
        else if (rnext.pointInside(where))
        {
            isWTHover = NEXT;
        }
        else if (rmenu.pointInside(where))
        {
            isWTHover = MENU;
        }
    }

    if (customEditorActive && customEditor)
        customEditor->onMouseEntered(where, buttons);

    invalid();
    return kMouseEventHandled;
}
CMouseEventResult COscillatorDisplay::onMouseExited(CPoint &where, const CButtonState &buttons)
{
    if (canHaveCustomEditor() && !customEditButtonRect.pointInside(where))
    {
        if (editButtonHover)
        {
            editButtonHover = false;
            invalid();
        }
    }

    isWTHover = NONE;
    invalid();
    if (customEditorActive && customEditor)
        customEditor->onMouseExited(where, buttons);
    return kMouseEventHandled;
}

/*
 * Custom Editor Support
 */
bool COscillatorDisplay::canHaveCustomEditor()
{
    if (oscdata->type.val.i == ot_alias &&
        oscdata->p[AliasOscillator::ao_wave].val.i == AliasOscillator::aow_additive)
    {
        return true;
    }

    return false;
}

void COscillatorDisplay::openCustomEditor()
{
    customEditorActive = false;
    if (!canHaveCustomEditor())
        return; // shouldn't happen but hey...

    struct AliasAdditive : public CustomEditor
    {
        explicit AliasAdditive(COscillatorDisplay *d) : CustomEditor(d) {}
        std::array<CRect, AliasOscillator::n_additive_partials> sliders;

        void draw(VSTGUI::CDrawContext *dc) override
        {
            auto vs = disp->getViewSize();
            auto divSize = vs;
            divSize.top += 12;
            divSize.bottom -= 11;
            auto w = divSize.getWidth() / AliasOscillator::n_additive_partials;

            dc->saveGlobalState();
            dc->setDrawMode(VSTGUI::kAntiAliasing | VSTGUI::kNonIntegralMode);

            for (int i = 0; i < AliasOscillator::n_additive_partials; ++i)
            {
                auto v = limit_range(disp->oscdata->extraConfig.data[i], -1.f, 1.f);
                auto p = CRect(CPoint(divSize.left + i * w, divSize.top),
                               CPoint(w, divSize.getHeight()));

                sliders[i] = p;

                auto q = p;
                q.top = q.bottom - p.getHeight() / 2 * (1 + v);
                q.bottom = q.bottom - p.getHeight() / 2;

                if (q.top < q.bottom)
                {
                    std::swap(q.bottom, q.top);
                }

                dc->setFillColor(disp->skin->getColor(Colors::Osc::Display::Wave));
                dc->drawRect(q, kDrawFilled);

                dc->setFrameColor(disp->skin->getColor(Colors::Osc::Display::Bounds));

                if (i != 0)
                {
                    dc->drawLine(p.getTopLeft(), p.getBottomLeft().offset(0, -1));
                }

                /*
                 * For now don't draw hovers
                if (i == hoveredSegment)
                {
                    // FIXME probably not this color
                    dc->setFillColor(disp->skin->getColor(Colors::Osc::Filename::BackgroundHover));
                    dc->drawRect(q, kDrawFilled);
                }
                 */
            }

            auto midpoint = divSize.top + (divSize.getHeight() / 2);
            auto midl = CPoint(divSize.left, midpoint);
            auto midr = CPoint(divSize.right, midpoint);

            dc->drawLine(midl, midr);

            dc->setFrameColor(disp->skin->getColor(Colors::Osc::Display::Center));
            dc->drawRect(divSize, kDrawStroked);

            dc->restoreGlobalState();
        }

        CMouseEventResult onMouseDown(CPoint &where, const CButtonState &buttons) override
        {
            dragMode = NONE;

            if (buttons & (kDoubleClick | kControl))
            {
                int i = 0;

                for (auto &s : sliders)
                {
                    if (s.pointInside(where))
                    {
                        disp->oscdata->extraConfig.data[i] = 0;
                        disp->invalid();
                    }
                    i++;
                }
            }

            if (buttons & kRButton)
            {
                auto contextMenu = new COptionMenu(CRect(where, CPoint(0, 0)), 0, 0, 0, 0,
                                                   COptionMenu::kMultipleCheckStyle);

                {
                    auto actionItem = new CCommandMenuItem(
                        CCommandMenuItem::Desc("[?] Alias Osc Additive Options"));
                    contextMenu->addEntry(actionItem);
                    contextMenu->addSeparator();
                }
                {
                    auto actionItem = new CCommandMenuItem(
                        CCommandMenuItem::Desc(Surge::UI::toOSCaseForMenu("Sine")));
                    auto action = [this](CCommandMenuItem *item) {
                        for (int qq = 0; qq < AliasOscillator::n_additive_partials; ++qq)
                        {
                            disp->oscdata->extraConfig.data[qq] = (qq == 0) ? 1 : 0;
                        }
                    };
                    actionItem->setActions(action, nullptr);
                    contextMenu->addEntry(actionItem);
                }
                {
                    auto actionItem = new CCommandMenuItem(
                        CCommandMenuItem::Desc(Surge::UI::toOSCaseForMenu("Triangle")));
                    auto action = [this](CCommandMenuItem *item) {
                        for (int qq = 0; qq < AliasOscillator::n_additive_partials; ++qq)
                        {
                            disp->oscdata->extraConfig.data[qq] =
                                (qq % 2 == 0) * 1.f / ((qq + 1) * (qq + 1));
                            if (qq % 4 == 2)
                                disp->oscdata->extraConfig.data[qq] *= -1.f;
                        }
                    };
                    actionItem->setActions(action, nullptr);
                    contextMenu->addEntry(actionItem);
                }
                {
                    auto actionItem = new CCommandMenuItem(
                        CCommandMenuItem::Desc(Surge::UI::toOSCaseForMenu("Sawtooth")));
                    auto action = [this](CCommandMenuItem *item) {
                        for (int qq = 0; qq < AliasOscillator::n_additive_partials; ++qq)
                        {
                            disp->oscdata->extraConfig.data[qq] = 1.f / (qq + 1);
                        }
                    };
                    actionItem->setActions(action, nullptr);
                    contextMenu->addEntry(actionItem);
                }
                {
                    auto actionItem = new CCommandMenuItem(
                        CCommandMenuItem::Desc(Surge::UI::toOSCaseForMenu("Square")));
                    auto action = [this](CCommandMenuItem *item) {
                        for (int qq = 0; qq < AliasOscillator::n_additive_partials; ++qq)
                        {
                            disp->oscdata->extraConfig.data[qq] = (qq % 2 == 0) * 1.f / (qq + 1);
                        }
                    };
                    actionItem->setActions(action, nullptr);
                    contextMenu->addEntry(actionItem);
                }
                {
                    auto actionItem = new CCommandMenuItem(
                        CCommandMenuItem::Desc(Surge::UI::toOSCaseForMenu("Random")));
                    auto action = [this](CCommandMenuItem *item) {
                        for (int qq = 0; qq < AliasOscillator::n_additive_partials; ++qq)
                        {
                            disp->oscdata->extraConfig.data[qq] = disp->storage->rand_pm1();
                        }
                    };
                    actionItem->setActions(action, nullptr);
                    contextMenu->addEntry(actionItem);
                }

                contextMenu->addSeparator();

                {
                    auto actionItem = new CCommandMenuItem(
                        CCommandMenuItem::Desc(Surge::UI::toOSCaseForMenu("Absolute")));
                    auto action = [this](CCommandMenuItem *item) {
                        for (int qq = 0; qq < AliasOscillator::n_additive_partials; ++qq)
                        {
                            if (disp->oscdata->extraConfig.data[qq] < 0)
                            {
                                disp->oscdata->extraConfig.data[qq] *= -1;
                            }
                        }
                    };
                    actionItem->setActions(action, nullptr);
                    contextMenu->addEntry(actionItem);
                }
                {
                    auto actionItem = new CCommandMenuItem(
                        CCommandMenuItem::Desc(Surge::UI::toOSCaseForMenu("Invert")));
                    auto action = [this](CCommandMenuItem *item) {
                        for (int qq = 0; qq < AliasOscillator::n_additive_partials; ++qq)
                        {
                            disp->oscdata->extraConfig.data[qq] =
                                -disp->oscdata->extraConfig.data[qq];
                        }
                    };
                    actionItem->setActions(action, nullptr);
                    contextMenu->addEntry(actionItem);
                }
                {
                    auto actionItem = new CCommandMenuItem(
                        CCommandMenuItem::Desc(Surge::UI::toOSCaseForMenu("Reverse")));
                    auto action = [this](CCommandMenuItem *item) {
                        float pdata[AliasOscillator::n_additive_partials];

                        for (int qq = 0; qq < AliasOscillator::n_additive_partials; ++qq)
                        {
                            pdata[qq] = disp->oscdata->extraConfig.data[qq];
                        }

                        for (int qq = 0; qq < AliasOscillator::n_additive_partials; ++qq)
                        {
                            disp->oscdata->extraConfig.data[15 - qq] = pdata[qq];
                        }
                    };
                    actionItem->setActions(action, nullptr);
                    contextMenu->addEntry(actionItem);
                }

                disp->getFrame()->addView(contextMenu); // add to frame
                contextMenu->setDirty();
                contextMenu->popup();
#if !TARGET_JUCE_UI
                // wth is this?
                contextMenu->onMouseDown(where, kLButton); // <-- modal menu loop is here
#endif
                // getFrame()->looseFocus(pContext);

                disp->getFrame()->removeView(contextMenu, true); // remove from frame and forget
                return kMouseDownEventHandledButDontNeedMovedOrUpEvents;
            }

            dragMode = EDIT;
            disp->startCursorHide();
            return onMouseMoved(where, buttons);
        }

        CMouseEventResult onMouseMoved(CPoint &where, const CButtonState &buttons) override
        {
            int ohs = hoveredSegment;
            int nhs = 0;

            for (auto &s : sliders)
            {
                if (s.pointInside(where))
                {
                    hoveredSegment = nhs;
                }
                nhs++;
            }

            if (hoveredSegment != ohs)
            {
                disp->invalid();
            }

            if (dragMode == EDIT)
            {
                int i = 0;

                for (auto &s : sliders)
                {
                    if ((where.x >= s.left) && (where.x <= s.right))
                    {
                        float f = (where.y - s.bottom) / (s.top - s.bottom);
                        f = (f - 0.5) * 2;

                        clamp1bp(f);

                        if ((buttons & kControl) || (buttons & kDoubleClick))
                        {
                            f = 0;
                        }

                        disp->oscdata->extraConfig.data[i] = f;
                        disp->invalid();
                        return kMouseEventHandled;
                    }
                    i++;
                }
            }

            return kMouseEventHandled;
        }

        CMouseEventResult onMouseExited(CPoint &where, const CButtonState &buttons) override
        {
            return kMouseEventHandled;
        }
        CMouseEventResult onMouseEntered(CPoint &where, const CButtonState &buttons) override
        {
            return kMouseEventHandled;
        }
        CMouseEventResult onMouseUp(CPoint &where, const CButtonState &buttons) override
        {
            dragMode = NONE;
            disp->endCursorHide(where);
            return kMouseEventHandled;
        }

        bool onWheel(const VSTGUI::CPoint &where, const float &distance,
                     const VSTGUI::CButtonState &buttons) override
        {
            int idx = 0;
            for (auto &s : sliders)
            {
                if (s.pointInside(where))
                {
                    auto f = disp->oscdata->extraConfig.data[idx];
                    if (buttons & kShift)
                        f += 0.1 * distance / 3;
                    else
                        f += 0.1 * distance;
                    f = limit_range(f, -1.f, 1.f);
                    disp->oscdata->extraConfig.data[idx] = f;
                    disp->invalid();
                }
                idx++;
            }
            return true;
        }

        int hoveredSegment = -1;
        enum DragMode
        {
            NONE,
            EDIT
        } dragMode = NONE;
    };

    if (oscdata->type.val.i == ot_alias &&
        oscdata->p[AliasOscillator::ao_wave].val.i == AliasOscillator::aow_additive)
    {
        customEditor = std::make_shared<AliasAdditive>(this);
        customEditorActive = true;
        storage->getPatch()
            .dawExtraState.editor.oscExtraEditState[scene][osc_in_scene]
            .hasCustomEditor = true;
        return;
    }

    storage->getPatch()
        .dawExtraState.editor.oscExtraEditState[scene][osc_in_scene]
        .hasCustomEditor = false;
}

void COscillatorDisplay::closeCustomEditor()
{
    customEditorActive = false;
    customEditor = nullptr;
    storage->getPatch()
        .dawExtraState.editor.oscExtraEditState[scene][osc_in_scene]
        .hasCustomEditor = false;
}
void COscillatorDisplay::drawExtraEditButton(CDrawContext *dc, const std::string &label)
{
    auto size = getViewSize();
    CRect posn(size);
    // Position this button just below the bottom bounds line
    posn.bottom = getHeight() + posn.top - 1;
    posn.top = posn.bottom - wtbheight + 4;
    posn.right = posn.left + 45;

    customEditButtonRect = posn;

    if (editButtonHover)
    {
        dc->setFillColor(skin->getColor(Colors::Osc::Filename::BackgroundHover));
        dc->setFrameColor(skin->getColor(Colors::Osc::Filename::FrameHover));
        dc->setFontColor(skin->getColor(Colors::Osc::Filename::TextHover));
    }
    else
    {
        dc->setFillColor(skin->getColor(Colors::Osc::Filename::Background));
        dc->setFrameColor(skin->getColor(Colors::Osc::Filename::Frame));
        dc->setFontColor(skin->getColor(Colors::Osc::Filename::Text));
    }

    dc->drawRect(posn, kDrawFilledAndStroked);
    dc->setFont(Surge::GUI::getLatoAtSize(7, kBoldFace));
    dc->drawString(label.c_str(), posn, kCenterText, true);
};
