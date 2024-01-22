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

#ifndef SURGE_SRC_SURGE_XT_GUI_OVERLAYS_TUNINGOVERLAYS_H
#define SURGE_SRC_SURGE_XT_GUI_OVERLAYS_TUNINGOVERLAYS_H

#include "Tunings.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <sstream>
#include <set>
#include "OverlayComponent.h"
#include "SkinSupport.h"
#include <bitset>

class SurgeStorage;
class SurgeGUIEditor;

namespace Surge
{
namespace Overlays
{

/*
 * To Dos
 * - skin everywhere
 * - class to struct
 * - edits edit consistently
 * - drop onto surge and voila
 * - etc
 */

class TuningTableListBoxModel;
class SCLKBMDisplay;
class RadialScaleGraph;
struct IntervalMatrix;

struct TuningControlArea;

struct TuningOverlay : public OverlayComponent,
                       public Surge::GUI::SkinConsumingComponent,
                       public juce::FileDragAndDropTarget
{
    class TuningTextEditedListener
    {
      public:
        virtual ~TuningTextEditedListener() = default;
        virtual void scaleTextEdited(juce::String newScale) = 0;
        // Do Mapping
    };

    TuningOverlay();
    ~TuningOverlay();

    void setStorage(SurgeStorage *s);

    SurgeGUIEditor *editor{nullptr};
    void setEditor(SurgeGUIEditor *ed) { editor = ed; }

    void onNewSCLKBM(const std::string &scl, const std::string &kbm);
    void onToneChanged(int tone, double newCentsValue);
    void onToneStringChanged(int tone, const std::string &newCentsValue);
    void onScaleRescaled(double scaledBy);
    void onScaleRescaledAbsolute(double setRITo);
    void recalculateScaleText();
    void setTuning(const Tunings::Tuning &t);
    void resetParentTitle();

    void resized() override;
    void visibilityChanged() override { resetParentTitle(); }

    void setMidiOnKeys(const std::bitset<128> &keys);

    void onSkinChanged() override;
    void onTearOutChanged(bool isTornOut) override;

    bool doDnD{false};
    bool isInterestedInFileDrag(const juce::StringArray &files) override;
    void filesDropped(const juce::StringArray &files, int x, int y) override;

    bool shouldRepaintOnParamChange(const SurgePatch &patch, Parameter *p) override
    {
        return false;
    }

    std::unique_ptr<TuningTableListBoxModel> tuningKeyboardTableModel;
    std::unique_ptr<juce::TableListBox> tuningKeyboardTable;

    std::unique_ptr<TuningControlArea> controlArea;

    void showEditor(int which);
    std::unique_ptr<SCLKBMDisplay> sclKbmDisplay;
    std::unique_ptr<RadialScaleGraph> radialScaleGraph;
    std::unique_ptr<IntervalMatrix> intervalMatrix;

    Tunings::Tuning tuning;
    SurgeStorage *storage{nullptr};

    bool mtsMode{false};
    void setMTSMode(bool isMTSOn);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TuningOverlay);
};
} // namespace Overlays
} // namespace Surge

#endif // SURGE_SRC_SURGE_XT_GUI_OVERLAYS_TUNINGOVERLAYS_H
