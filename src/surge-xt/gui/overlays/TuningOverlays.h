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

class SurgeStorage;

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

struct TuningOverlay : public OverlayComponent, public Surge::GUI::SkinConsumingComponent
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

    void setStorage(SurgeStorage *s) { storage = s; }

    void setTuning(const Tunings::Tuning &t);
    void resized() override;

    void onSkinChanged() override;

    std::unique_ptr<TuningTableListBoxModel> tuningKeyboardTableModel;
    std::unique_ptr<juce::TableListBox> tuningKeyboardTable;
    std::unique_ptr<juce::TabbedComponent> tabArea;

    std::unique_ptr<SCLKBMDisplay> sclKbmDisplay;
    std::unique_ptr<RadialScaleGraph> radialScaleGraph;

    Tunings::Tuning tuning;
    SurgeStorage *storage{nullptr};
};
} // namespace Overlays
} // namespace Surge
