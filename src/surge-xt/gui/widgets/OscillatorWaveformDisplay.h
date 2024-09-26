/*
 * Surge XT - a free and open source hybrid synthesizer,
 * built by Surge Synth Team
 *
 * Learn more at https://surge-synthesizer.github.io/
 *
 * Copyright 2018-2024, various authors, as described in the GitHub
 * transaction log.
 *
 * Surge XT is released under the GNU General Public Licence v3
 * or later (GPL-3.0-or-later). The license is found in the "LICENSE"
 * file in the root of this repository, or at
 * https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 * Surge was a commercial product from 2004-2018, copyright and ownership
 * held by Claes Johanson at Vember Audio during that period.
 * Claes made Surge open source in September 2018.
 *
 * All source for Surge XT is available at
 * https://github.com/surge-synthesizer/surge
 */

#ifndef SURGE_SRC_SURGE_XT_GUI_WIDGETS_OSCILLATORWAVEFORMDISPLAY_H
#define SURGE_SRC_SURGE_XT_GUI_WIDGETS_OSCILLATORWAVEFORMDISPLAY_H

#include "SkinSupport.h"
#include "Parameter.h"
#include "SurgeStorage.h"
#include "WidgetBaseMixin.h"

#include "juce_gui_basics/juce_gui_basics.h"
#include "Oscillator.h"
#include "AccessibleHelpers.h"

class SurgeStorage;
class SurgeGUIEditor;
class OscillatorStorage;
class Oscillator;

namespace Surge
{
namespace Widgets
{
struct OscillatorWaveformDisplay;
template <> void LongHoldMixin<OscillatorWaveformDisplay>::onLongHold();

struct OscillatorWaveformDisplay : public juce::Component,
                                   public Surge::GUI::SkinConsumingComponent,
                                   public Surge::Widgets::LongHoldMixin<OscillatorWaveformDisplay>,
                                   public Surge::GUI::Hoverable
{
    OscillatorWaveformDisplay();
    ~OscillatorWaveformDisplay();

    static constexpr float disp_pitch = 90.15f - 48.f;
    static constexpr int wtbheight = 12;
    static constexpr float scaleDownBy = 0.235;

    SurgeStorage *storage{nullptr};
    void setStorage(SurgeStorage *s) { storage = s; }

    OscillatorStorage *oscdata{nullptr};
    int oscInScene{-1};
    int scene{-1};
    void setOscStorage(OscillatorStorage *s)
    {
        oscdata = s;
        scene = oscdata->p[0].scene - 1;
        oscInScene = oscdata->p[0].ctrlgroup_entry;
    }

    bool isMuted{false};
    bool forceWTRepaint{false};

    SurgeGUIEditor *sge{nullptr};
    void setSurgeGUIEditor(SurgeGUIEditor *s) { sge = s; }

    void onOscillatorTypeChanged();
    std::string getCurrentWavetableName();

    void repaintIfIdIsInRange(int id);
    void repaintBasedOnOscMuteState();
    void repaintForceForWT() { forceWTRepaint = true; };

    ::Oscillator *setupOscillator();
    unsigned char oscbuffer alignas(16)[oscillator_buffer_size];

    void paint(juce::Graphics &g) override;
    void resized() override;

    void setZoomFactor(int);

    pdata tp[n_scene_params];
    juce::Rectangle<float> leftJog, rightJog, waveTableName;

    void mouseDown(const juce::MouseEvent &event) override;
    void mouseUp(const juce::MouseEvent &event) override;
    void mouseMove(const juce::MouseEvent &event) override;

    bool isMousedOver{false};
    void mouseEnter(const juce::MouseEvent &event) override;
    void mouseExit(const juce::MouseEvent &event) override;

    bool keyPressed(const juce::KeyPress &key) override;

    void loadWavetable(int id);
    void loadWavetableFromFile();
    void populateMenu(juce::PopupMenu &m, int selectedItem, bool singleCategory = false);
    bool populateMenuForCategory(juce::PopupMenu &parent, int categoryId, int selectedItem,
                                 bool intoTop = false);
    void showWavetableMenu(bool singleCategory = false);
    void createWTMenu(const bool useComponentBounds);
    void createWTMenuItems(juce::PopupMenu &contextMenu, bool centered = false,
                           bool add2D3Dswitch = false);

    void createAliasOptionsMenu(const bool useComponentBounds = false,
                                const bool onlyHelpEntry = false);

    bool isCustomEditorHovered{false}, isJogRHovered{false}, isJogLHovered{false},
        isWtNameHovered{false};

    bool isCurrentlyHovered() override { return isMousedOver || isCustomEditorHovered; }

    juce::Rectangle<float> customEditorBox;
    bool supportsCustomEditor();
    void showCustomEditor();
    void hideCustomEditor();
    void toggleCustomEditor();
    void drawEditorBox(juce::Graphics &g, const std::string &s);
    bool isCustomEditAccessible() const;
    std::string customEditorActionLabel(bool isActionToOpen) const;
    static constexpr int wt3DControlWidth = 20;
    std::unique_ptr<juce::Component> customEditor;

    std::array<std::unique_ptr<juce::Component>, 3> menuOverlays;
    std::unique_ptr<juce::Component> customEditorAccOverlay;
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;
    int lastWavetableId{-1};
    std::string lastWavetableFilename;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OscillatorWaveformDisplay);
};

template <> inline void LongHoldMixin<OscillatorWaveformDisplay>::onLongHold()
{
    asT()->createWTMenu(true);
}

} // namespace Widgets
} // namespace Surge

#endif // SURGE_XT_OSCILLATORWAVEFORMDISPLAY_H
