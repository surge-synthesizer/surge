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

#include "OscillatorWaveformDisplay.h"
#include "SurgeStorage.h"
#include "Oscillator.h"
#include "OscillatorBase.h"
#include "RuntimeFont.h"
#include "SurgeGUIUtils.h"
#include "SurgeGUIEditor.h"
#include "AliasOscillator.h"

namespace Surge
{
namespace Widgets
{
OscillatorWaveformDisplay::OscillatorWaveformDisplay() = default;
OscillatorWaveformDisplay::~OscillatorWaveformDisplay() = default;

void OscillatorWaveformDisplay::paint(juce::Graphics &g)
{
    if (supportsCustomEditor() && customEditor)
    {
        drawEditorBox(g, "CLOSE");
        return;
    }

    if (!supportsCustomEditor() && customEditor)
    {
        dismissCustomEditor();
    }

    bool usesWT = uses_wavetabledata(oscdata->type.val.i);

    auto osc = setupOscillator();
    if (!osc)
        return;

    int totalSamples = (1 << 4) * (int)getWidth();
    int averagingWindow = 4; // < and Mult of BlockSizeOS

    float disp_pitch_rs = disp_pitch + 12.0 * log2(dsamplerate / 44100.0);
    if (!storage->isStandardTuning)
    {
        // OK so in this case we need to find a better version of the note which gets us
        // that pitch. Only way is to search really.
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
        // That's a strange non-monotonic tuning. Oh well.
    }

    bool use_display = osc->allow_display();

    if (use_display)
        osc->init(disp_pitch_rs, true, true);

    int block_pos = BLOCK_SIZE_OS;
    juce::Path wavePath;
    for (int i = 0; i < totalSamples; i += averagingWindow)
    {
        if (use_display && block_pos >= BLOCK_SIZE_OS)
        {
            // Lock it even if we aren't wavetable. It's fine.
            storage->waveTableDataMutex.lock();
            osc->process_block(disp_pitch_rs);
            block_pos = 0;
            storage->waveTableDataMutex.unlock();
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
        }
        float xc = 1.f * i / totalSamples;

        if (i == 0)
        {
            wavePath.startNewSubPath(xc, val);
        }
        else
        {
            wavePath.lineTo(xc, val);
        }
    }

    auto yMargin = 2 * usesWT;
    auto h = getHeight() - usesWT * wtbheight - 2 * yMargin;
    auto xMargin = 2;
    auto w = getWidth() - 2 * xMargin;
    auto downScale = 0.235;
    auto tf = juce::AffineTransform()
                  .scaled(w, -(1 - downScale) * h / 2)
                  .translated(xMargin, h / 2 + yMargin);

    // Draw the lines
    auto tfLine = [tf](float x0, float y0, float x1, float y1) -> juce::Line<float> {
        auto p0 = juce::Point<float>(x0, y0).transformedBy(tf);
        auto p1 = juce::Point<float>(x1, y1).transformedBy(tf);
        return juce::Line<float>(p0, p1);
    };

    g.setColour(skin->getColor(Colors::Osc::Display::Bounds));
    g.drawLine(tfLine(0, -1, 1, -1));
    g.drawLine(tfLine(0, 1, 1, 1));

    g.setColour(juce::Colours::green);
    g.setColour(skin->getColor(Colors::Osc::Display::Center));
    g.drawLine(tfLine(0, 0, 1, 0));

    g.setColour(skin->getColor(Colors::Osc::Display::Dots));
    int nxd = 21, nyd = 13;
    for (int xd = 0; xd < nxd; ++xd)
    {
        float normx = 1.f * xd / (nxd - 1);
        for (int yd = 0; yd < nyd; ++yd)
        {
            float normy = 2.f * yd / (nyd - 1) - 1;

            auto p = juce::Point<float>(normx, normy).transformedBy(tf);
            g.fillEllipse(p.x - 0.5, p.y - 0.5, 1, 1);
        }
    }

    g.setColour(skin->getColor(Colors::Osc::Display::Wave));
    g.strokePath(wavePath, juce::PathStrokeType(1.3), tf);

    if (usesWT)
    {
        auto fgcol = skin->getColor(Colors::Osc::Filename::Background);
        auto fgframe = skin->getColor(Colors::Osc::Filename::Frame);
        auto fgtext = skin->getColor(Colors::Osc::Filename::Text);

        auto fgcolHov = skin->getColor(Colors::Osc::Filename::BackgroundHover);
        auto fgframeHov = skin->getColor(Colors::Osc::Filename::FrameHover);
        auto fgtextHov = skin->getColor(Colors::Osc::Filename::TextHover);

        auto wtr = getLocalBounds().withTop(getHeight() - wtbheight).withTrimmedBottom(1).toFloat();
        g.setColour(juce::Colours::orange);
        g.fillRect(wtr);

        leftJog = wtr.withRight(wtbheight);
        rightJog = wtr.withLeft(wtr.getWidth() - wtbheight);
        waveTableName = wtr.withTrimmedRight(wtbheight).withTrimmedLeft(wtbheight);

        float triO = 2;
        g.setColour(isJogLHovered ? fgcolHov : fgcol);
        g.fillRect(leftJog);
        g.setColour(isJogLHovered ? fgframeHov : fgframe);
        g.drawRect(leftJog);
        g.setColour(isJogLHovered ? fgtextHov : fgtext);
        auto triL = juce::Path();
        triL.addTriangle(leftJog.getTopRight().translated(-triO, triO),
                         leftJog.getBottomRight().translated(-triO, -triO),
                         leftJog.getCentre().withX(leftJog.getX() + triO));
        g.fillPath(triL);

        g.setColour(isJogRHovered ? fgcolHov : fgcol);
        g.fillRect(rightJog);
        g.setColour(isJogRHovered ? fgframeHov : fgframe);
        g.drawRect(rightJog);
        g.setColour(isJogRHovered ? fgtextHov : fgtext);
        auto triR = juce::Path();
        triR.addTriangle(rightJog.getTopLeft().translated(triO, triO),
                         rightJog.getBottomLeft().translated(triO, -triO),
                         rightJog.getCentre().withX(rightJog.getX() + rightJog.getWidth() - triO));
        g.fillPath(triR);

        g.setColour(isWtNameHovered ? fgcolHov : fgcol);
        g.fillRect(waveTableName);
        g.setColour(isWtNameHovered ? fgframeHov : fgframe);
        g.drawRect(waveTableName);

        g.setFont(Surge::GUI::getFontManager()->getLatoAtSize(9));
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

        g.setColour(isWtNameHovered ? fgtextHov : fgtext);
        g.drawText(wttxt, waveTableName, juce::Justification::centred);
    }

    if (supportsCustomEditor())
    {
        drawEditorBox(g, "EDIT");
    }
}

void OscillatorWaveformDisplay::repaintIfIdIsInRange(int id)
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
        repaint();
    }
}

std::unique_ptr<::Oscillator> OscillatorWaveformDisplay::setupOscillator()
{
    tp[oscdata->pitch.param_id_in_scene].f = 0;
    for (int i = 0; i < n_osc_params; i++)
        tp[oscdata->p[i].param_id_in_scene].i = oscdata->p[i].val.i;
    return std::unique_ptr<::Oscillator>(spawn_osc(oscdata->type.val.i, storage, oscdata, tp));
}

void OscillatorWaveformDisplay::populateMenu(juce::PopupMenu &contextMenu, int selectedItem)
{
    int idx = 0;

    contextMenu.addSectionHeader("FACTORY WAVETABLES");

    for (auto c : storage->wtCategoryOrdering)
    {
        if (idx == storage->firstThirdPartyWTCategory)
        {
            contextMenu.addSectionHeader("3RD PARTY WAVETABLES");
        }

        if (idx == storage->firstUserWTCategory &&
            storage->firstUserWTCategory != storage->wt_category.size())
        {
            contextMenu.addSectionHeader("USER WAVETABLES");
        }

        idx++;

        PatchCategory cat = storage->wt_category[c];
        if (cat.numberOfPatchesInCategoryAndChildren == 0)
            continue;

        if (cat.isRoot)
        {
            populateMenuForCategory(contextMenu, c, selectedItem);
        }
    }

    contextMenu.addSeparator();
    auto owts = [this]() {
        if (sge)
            sge->showWavetableScripter();
    };

    contextMenu.addItem(Surge::GUI::toOSCaseForMenu("Wavetable Editor..."), owts);
    contextMenu.addSeparator();

    auto refresh = [this]() { this->storage->refresh_wtlist(); };
    contextMenu.addItem(Surge::GUI::toOSCaseForMenu("Refresh Wavetable List"), refresh);

    auto rnaction = [this]() {
        char c[256];
        strncpy(c, this->oscdata->wavetable_display_name, 256);
        if (sge)
        {
            sge->promptForMiniEdit(
                c, "Enter a custom wavetable display name:", "Wavetable Display Name",
                VSTGUI::CPoint(-1, -1), [this](const std::string &s) {
                    strncpy(this->oscdata->wavetable_display_name, s.c_str(), 256);
                    this->repaint();
                });
        }
    };
    contextMenu.addItem(Surge::GUI::toOSCaseForMenu("Change Wavetable Display Name..."), rnaction);

    contextMenu.addSeparator();

    auto action = [this]() { this->loadWavetableFromFile(); };
    contextMenu.addItem(Surge::GUI::toOSCaseForMenu("Load Wavetable from File..."), action);

    auto exportAction = [this]() {
        int oscNum = this->oscInScene;
        int scene = this->scene;

        std::string baseName = storage->getPatch().name + "_osc" + std::to_string(oscNum + 1) +
                               "_scene" + (scene == 0 ? "A" : "B");
        auto fn = storage->export_wt_wav_portable(baseName, &(oscdata->wt));
        if (!fn.empty())
        {
            std::string message = "Wavetable was successfully exported to '" + fn + "'";
            juce::AlertWindow::showMessageBox(juce::AlertWindow::InfoIcon, "Wavetable Exported!",
                                              message);
        }
    };
    contextMenu.addItem(Surge::GUI::toOSCaseForMenu("Save Wavetable to File..."), exportAction);

    contextMenu.addItem(Surge::GUI::toOSCaseForMenu("Open Exported Wavetables Folder..."),
                        [this]() {
                            Surge::GUI::openFileOrFolder(Surge::Storage::appendDirectory(
                                this->storage->userDataPath, "Exported Wavetables"));
                        });

    if (sge)
    {
        auto hu = sge->helpURLForSpecial("wavetables");
        if (hu != "")
        {
            auto lurl = sge->fullyResolvedHelpURL(hu);
            auto ca = [lurl]() { juce::URL(lurl).launchInDefaultBrowser(); };
            contextMenu.addSeparator();
            contextMenu.addItem("[?] Wavetables", ca);
        }
    }
}

bool OscillatorWaveformDisplay::populateMenuForCategory(juce::PopupMenu &contextMenu,
                                                        int categoryId, int selectedItem)
{
    char name[NAMECHARS];
    juce::PopupMenu subMenu;
    int sub = 0;

    PatchCategory cat = storage->wt_category[categoryId];

    for (auto p : storage->wtOrdering)
    {
        if (storage->wt_list[p].category == categoryId)
        {
            sprintf(name, "%s", storage->wt_list[p].name.c_str());
            auto action = [this, p]() { this->loadWavetable(p); };

            bool checked = (p == selectedItem);
            subMenu.addItem(name, true, checked, action);

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

    contextMenu.addSubMenu(name, subMenu);

#if 0
    if (selected || (selectedItem >= 0 && storage->wt_list[selectedItem].category == categoryId))
    {
        selected = true;
        submenuItem->setChecked(true);
    }
#endif

    return selected;
}

void OscillatorWaveformDisplay::loadWavetable(int id)
{
    if (id >= 0 && (id < storage->wt_list.size()))
    {
        oscdata->wt.queue_id = id;
    }
}

void OscillatorWaveformDisplay::loadWavetableFromFile()
{
    juce::FileChooser c("Select Wavetable to Load");
    auto r = c.browseForFileToOpen();
    if (r)
    {
        auto res = c.getResult();
        auto rString = res.getFullPathName().toStdString();
        strncpy(this->oscdata->wt.queue_filename, rString.c_str(), 255);
    }
}
void OscillatorWaveformDisplay::mouseUp(const juce::MouseEvent &event)
{
    bool usesWT = uses_wavetabledata(oscdata->type.val.i);

    if (usesWT)
    {
        int id = oscdata->wt.current_id;
        if (leftJog.contains(event.position))
        {
            id = storage->getAdjacentWaveTable(oscdata->wt.current_id, false);
            if (id >= 0)
                oscdata->wt.queue_id = id;
        }
        else if (rightJog.contains(event.position))
        {
            id = storage->getAdjacentWaveTable(oscdata->wt.current_id, true);
            if (id >= 0)
                oscdata->wt.queue_id = id;
        }
        else if (waveTableName.contains(event.position))
        {
            juce::PopupMenu menu;
            populateMenu(menu, id);
            menu.showMenuAsync(juce::PopupMenu::Options());
        }
    }

    if (supportsCustomEditor() && customEditorBox.contains(event.position))
    {
        if (customEditor)
        {
            dismissCustomEditor();
        }
        else
        {
            showCustomEditor();
        }
    }
}
void OscillatorWaveformDisplay::mouseMove(const juce::MouseEvent &event)
{
    if (supportsCustomEditor())
    {
        auto q = customEditorBox.contains(event.position);
        if (q != isCustomEditorHovered)
        {
            isCustomEditorHovered = q;
            repaint();
        }
    }
    else
    {
        isCustomEditorHovered = false;
    }

    bool usesWT = uses_wavetabledata(oscdata->type.val.i);
    if (usesWT)
    {
        auto nwt = waveTableName.contains(event.position);
        auto njl = leftJog.contains(event.position);
        auto nrj = rightJog.contains(event.position);

        if (nwt != isWtNameHovered)
        {
            isWtNameHovered = nwt;
            repaint();
        }
        if (njl != isJogLHovered)
        {
            isJogLHovered = njl;
            repaint();
        }
        if (nrj != isJogRHovered)
        {
            isJogRHovered = nrj;
            repaint();
        }
    }
    else
    {
        isJogRHovered = false;
        isJogRHovered = false;
        isWtNameHovered = false;
    }
}

bool OscillatorWaveformDisplay::supportsCustomEditor()
{
    if (oscdata->type.val.i == ot_alias &&
        oscdata->p[AliasOscillator::ao_wave].val.i == AliasOscillator::aow_additive)
    {
        return true;
    }

    return false;
}

struct AliasAdditiveEditor : public juce::Component
{
    juce::Colour b = juce::Colours::pink;
    AliasAdditiveEditor(SurgeStorage *s, OscillatorStorage *osc, Surge::GUI::Skin::ptr_t sk)
        : storage(s), oscdata(osc), skin(sk)
    {
    }
    OscillatorStorage *oscdata;
    SurgeStorage *storage;
    Surge::GUI::Skin::ptr_t skin;

    std::array<juce::Rectangle<float>, AliasOscillator::n_additive_partials> sliders;

    void paint(juce::Graphics &g) override
    {
        auto w = 1.f * getWidth() / AliasOscillator::n_additive_partials;

        float halfHeight = getHeight() / 2.f;

        for (int i = 0; i < AliasOscillator::n_additive_partials; ++i)
        {
            auto v = limitpm1(oscdata->extraConfig.data[i]);
            auto p = juce::Rectangle<float>(i * w, 0, w, getHeight());

            sliders[i] = p;

            auto bar = p;

            if (v < 0)
            {
                bar = p.withTop(halfHeight).withBottom(halfHeight * (1 - v));
            }
            else
            {
                bar = p.withBottom(halfHeight).withTop(halfHeight * (1 - v));
            }

            g.setColour(skin->getColor(Colors::Osc::Display::Wave));
            g.fillRect(bar);
            g.setColour(skin->getColor(Colors::Osc::Display::Bounds));
            g.drawRect(sliders[i]);
        }

        g.drawLine(0.f, halfHeight, getWidth(), halfHeight);
    }

    void mouseDown(const juce::MouseEvent &event) override
    {
        if (event.mods.isPopupMenu())
        {
            auto contextMenu = juce::PopupMenu();
            {
                contextMenu.addItem("[?] Alias Osc Additive Options", []() {});
                contextMenu.addSeparator();
            }
            {
                auto action = [this]() {
                    for (int qq = 0; qq < AliasOscillator::n_additive_partials; ++qq)
                    {
                        oscdata->extraConfig.data[qq] = (qq == 0) ? 1 : 0;
                    }

                    repaint();
                };
                contextMenu.addItem("Sine", action);
            }
            {
                auto action = [this]() {
                    for (int qq = 0; qq < AliasOscillator::n_additive_partials; ++qq)
                    {
                        oscdata->extraConfig.data[qq] = (qq % 2 == 0) * 1.f / ((qq + 1) * (qq + 1));
                        if (qq % 4 == 2)
                            oscdata->extraConfig.data[qq] *= -1.f;
                    }

                    repaint();
                };
                contextMenu.addItem("Triangle", action);
            }
            {
                auto action = [this]() {
                    for (int qq = 0; qq < AliasOscillator::n_additive_partials; ++qq)
                    {
                        oscdata->extraConfig.data[qq] = 1.f / (qq + 1);
                    }

                    repaint();
                };
                contextMenu.addItem("Sawtooth", action);
            }
            {
                auto action = [this]() {
                    for (int qq = 0; qq < AliasOscillator::n_additive_partials; ++qq)
                    {
                        oscdata->extraConfig.data[qq] = (qq % 2 == 0) * 1.f / (qq + 1);
                    }

                    repaint();
                };
                contextMenu.addItem("Square", action);
            }
            {
                auto action = [this]() {
                    for (int qq = 0; qq < AliasOscillator::n_additive_partials; ++qq)
                    {
                        oscdata->extraConfig.data[qq] = storage->rand_pm1();
                    }

                    repaint();
                };
                contextMenu.addItem("Random", action);
            }

            contextMenu.addSeparator();

            {
                auto action = [this]() {
                    for (int qq = 0; qq < AliasOscillator::n_additive_partials; ++qq)
                    {
                        if (oscdata->extraConfig.data[qq] < 0)
                        {
                            oscdata->extraConfig.data[qq] *= -1;
                        }
                    }

                    repaint();
                };
                contextMenu.addItem("Absolute", action);
            }
            {
                auto action = [this]() {
                    for (int qq = 0; qq < AliasOscillator::n_additive_partials; ++qq)
                    {
                        oscdata->extraConfig.data[qq] = -oscdata->extraConfig.data[qq];
                    }
                };
                contextMenu.addItem("Invert", action);
                repaint();
            }
            {
                auto action = [this]() {
                    float pdata[AliasOscillator::n_additive_partials];

                    for (int qq = 0; qq < AliasOscillator::n_additive_partials; ++qq)
                    {
                        pdata[qq] = oscdata->extraConfig.data[qq];
                    }

                    for (int qq = 0; qq < AliasOscillator::n_additive_partials; ++qq)
                    {
                        oscdata->extraConfig.data[15 - qq] = pdata[qq];
                    }
                    repaint();
                };
                contextMenu.addItem("Reverse", action);
            }

            contextMenu.showMenuAsync(juce::PopupMenu::Options());
            return;
        }

        if (event.mods.isLeftButtonDown())
        {
            int clickedSlider = -1;

            for (int i = 0; i < AliasOscillator::n_additive_partials; ++i)
            {
                if (sliders[i].contains(event.position))
                {
                    clickedSlider = i;
                }
            }

            if (clickedSlider >= 0)
            {
                auto d = (-1.f * event.position.y / getHeight() + 0.5) * 2 *
                         (!event.mods.isCommandDown());
                oscdata->extraConfig.data[clickedSlider] = limitpm1(d);

                repaint();
            }
        }
    }

    void mouseDoubleClick(const juce::MouseEvent &event) override
    {
        int clickedSlider = -1;

        for (int i = 0; i < AliasOscillator::n_additive_partials; ++i)
        {
            if (sliders[i].contains(event.position))
            {
                clickedSlider = i;
            }
        }

        if (clickedSlider >= 0)
        {
            oscdata->extraConfig.data[clickedSlider] = 0.f;

            repaint();
        }
    }

    void mouseDrag(const juce::MouseEvent &event) override
    {

        int draggedSlider = -1;

        for (int i = 0; i < AliasOscillator::n_additive_partials; ++i)
        {
            if (sliders[i].contains(event.position))
            {
                draggedSlider = i;
            }
        }

        if (draggedSlider >= 0)
        {
            auto d =
                (-1.f * event.position.y / getHeight() + 0.5) * 2 * (!event.mods.isCommandDown());
            oscdata->extraConfig.data[draggedSlider] = limitpm1(d);

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
        {
            return;
        }

#if MAC
        float speed = 1.2;
#else
        float speed = 0.42666;
#endif

        if (event.mods.isShiftDown())
        {
            speed /= 10.f;
        }

        delta *= speed;

        int draggedSlider = -1;

        for (int i = 0; i < AliasOscillator::n_additive_partials; ++i)
        {
            if (sliders[i].contains(event.position))
            {
                draggedSlider = i;
            }
        }

        if (draggedSlider >= 0)
        {
            auto d = oscdata->extraConfig.data[draggedSlider] + delta;
            oscdata->extraConfig.data[draggedSlider] = limitpm1(d);
            repaint();
        }
    }
};

void OscillatorWaveformDisplay::showCustomEditor()
{
    if (customEditor)
        dismissCustomEditor();

    if (oscdata->type.val.i == ot_alias &&
        oscdata->p[AliasOscillator::ao_wave].val.i == AliasOscillator::aow_additive)
        customEditor = std::make_unique<AliasAdditiveEditor>(storage, oscdata, skin);

    if (customEditor)
    {
        auto b = getLocalBounds().withTrimmedBottom(wtbheight);
        customEditor->setBounds(b);
        addAndMakeVisible(*customEditor);
        repaint();
    }
}

void OscillatorWaveformDisplay::dismissCustomEditor()
{
    if (customEditor)
        removeChildComponent(customEditor.get());
    customEditor.reset(nullptr);
    repaint();
}

void OscillatorWaveformDisplay::drawEditorBox(juce::Graphics &g, const std::string &s)
{
    customEditorBox = getLocalBounds()
                          .withTop(getHeight() - wtbheight)
                          .withRight(60)
                          .withTrimmedBottom(1)
                          .toFloat();

    juce::Colour bg, outline, txt;
    if (isCustomEditorHovered)
    {
        bg = skin->getColor(Colors::Osc::Filename::BackgroundHover);
        outline = skin->getColor(Colors::Osc::Filename::FrameHover);
        txt = skin->getColor(Colors::Osc::Filename::TextHover);
    }
    else
    {
        bg = skin->getColor(Colors::Osc::Filename::Background);
        outline = skin->getColor(Colors::Osc::Filename::Frame);
        txt = skin->getColor(Colors::Osc::Filename::Text);
    }
    g.setColour(bg);
    g.fillRect(customEditorBox);
    g.setColour(outline);
    g.drawRect(customEditorBox);
    g.setColour(txt);
    g.setFont(Surge::GUI::getFontManager()->getLatoAtSize(9));
    g.drawText(s, customEditorBox, juce::Justification::centred);
}
void OscillatorWaveformDisplay::mouseExit(const juce::MouseEvent &event)
{
    isCustomEditorHovered = false;
    isJogLHovered = false;
    isJogRHovered = false;
    isWtNameHovered = false;
    repaint();
}

} // namespace Widgets
} // namespace Surge