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

#ifndef SURGE_XT_OSCILLATORWAVEFORMDISPLAY_H
#define SURGE_XT_OSCILLATORWAVEFORMDISPLAY_H

#include <JuceHeader.h>
#include "SkinSupport.h"
#include "Parameter.h"
#include "SurgeStorage.h"

class SurgeStorage;
class SurgeGUIEditor;
class OscillatorStorage;
class Oscillator;

namespace Surge
{
namespace Widgets
{
struct OscillatorWaveformDisplay : public juce::Component, public Surge::GUI::SkinConsumingComponent
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

    SurgeGUIEditor *sge;
    void setSurgeGUIEditor(SurgeGUIEditor *s) { sge = s; }

    void repaintIfIdIsInRange(int id);

    std::unique_ptr<::Oscillator> setupOscillator();

    void paint(juce::Graphics &g) override;

    pdata tp[n_scene_params];
    juce::Rectangle<float> leftJog, rightJog, waveTableName;

    void mouseUp(const juce::MouseEvent &event) override;
    void mouseMove(const juce::MouseEvent &event) override;
    void mouseExit(const juce::MouseEvent &event) override;

    void loadWavetable(int id);
    void loadWavetableFromFile();
    void populateMenu(juce::PopupMenu &m, int selectedItem);
    bool populateMenuForCategory(juce::PopupMenu &parent, int categoryId, int selectedItem);

    bool isCustomEditorHovered{false}, isJogRHovered{false}, isJogLHovered{false},
        isWtNameHovered{false};

    juce::Rectangle<float> customEditorBox;
    bool supportsCustomEditor();
    void showCustomEditor();
    void dismissCustomEditor();
    void drawEditorBox(juce::Graphics &g, const std::string &s);
    std::unique_ptr<juce::Component> customEditor;
};
} // namespace Widgets
} // namespace Surge

#endif // SURGE_XT_OSCILLATORWAVEFORMDISPLAY_H
