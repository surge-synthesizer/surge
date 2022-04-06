// -*-c++-*-
/*******************************************************************************
 BEGIN_JUCE_MODULE_DECLARATION

  ID:                 surgesynthteam_tuningui
  vendor:             surgesynthteam
  version:            1.0.0
  name:               Surge Synth Team JUCE UI Componenents for Tuning Synths
  description:        Various UI helpers for making rich tuning UIs
  website:            http://surge-synth-team.org/
  license:            GPL3

  dependencies:       juce_graphics juce_gui_basics

 END_JUCE_MODULE_DECLARATION

*******************************************************************************/

#pragma once

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
    void resized() override;

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TuningOverlay);
};
} // namespace Overlays
} // namespace Surge
