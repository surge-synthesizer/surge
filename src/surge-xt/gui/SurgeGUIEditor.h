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

#ifndef SURGE_SRC_SURGE_XT_GUI_SURGEGUIEDITOR_H
#define SURGE_SRC_SURGE_XT_GUI_SURGEGUIEDITOR_H

#include <deque>

#include "globals.h"

#include "SurgeGUICallbackInterfaces.h"

#include "SurgeStorage.h"
#include "SurgeImageStore.h"

#include "SurgeSynthesizer.h"

#include "SkinSupport.h"
#include "SkinColors.h"

#include "UserDefaults.h"

#include "overlays/OverlayComponent.h"
#include "overlays/MSEGEditor.h"
#include "overlays/OverlayWrapper.h" // This needs to be concrete for inline functions for now
#include "overlays/TuningOverlays.h"
#include "widgets/ModulatableControlInterface.h"

#include "juce_gui_basics/juce_gui_basics.h"

#include <vector>
#include <thread>
#include <atomic>
#include <cstdarg>
#include <bitset>
#include "UndoManager.h"

// Change this to 0 to disable WTSE component, to disable for release: change value, test, and push
#define INCLUDE_WT_SCRIPTING_EDITOR 1

class SurgeSynthEditor;

#if SURGE_INCLUDE_MELATONIN_INSPECTOR
namespace melatonin
{
class Inspector;
}
#endif

namespace Surge
{
namespace Widgets
{
struct EffectChooser;
struct EffectLabel;
struct LFOAndStepDisplay;
struct ModulationSourceButton;
struct ModulatableControlInterface;
struct ModulationOverviewLaunchButton;
struct NumberField;
struct OscillatorWaveformDisplay;
struct ParameterInfowindow;
struct PatchSelector;
struct PatchSelectorCommentTooltip;
struct Switch;
struct VerticalLabel;
struct VuMeter;

struct MainFrame;

struct WaveShaperSelector;
struct OscillatorMenu;
struct FxMenu;
} // namespace Widgets

namespace Overlays
{
struct AboutScreen;
struct TypeinParamEditor;
struct MiniEdit;
struct Alert;
struct OverlayWrapper;
struct PatchStoreDialog;
} // namespace Overlays
} // namespace Surge

#include "sst/plugininfra/keybindings.h"
namespace sst
{
namespace plugininfra
{
template <> inline std::string keyCodeToString<juce::KeyPress>(int keyCode)
{
    auto k = juce::KeyPress(keyCode);
    return k.getTextDescription().toStdString(); // according to the doc this is stable
}

template <> inline int keyCodeFromString<juce::KeyPress>(const std::string &s)
{
    auto k = juce::KeyPress::createFromDescription(s);
    return k.getKeyCode();
}
} // namespace plugininfra
} // namespace sst

#include "SurgeGUIEditorKeyboardActions.h"

class SurgeGUIEditor : public Surge::GUI::IComponentTagValue::Listener,
                       public SurgeStorage::ErrorListener,
                       public juce::KeyListener,
                       public juce::FocusChangeListener,
                       public SurgeSynthesizer::ModulationAPIListener
{
  public:
    SurgeGUIEditor(SurgeSynthEditor *juceEditor, SurgeSynthesizer *synth);
    virtual ~SurgeGUIEditor();

    std::unique_ptr<Surge::Widgets::MainFrame> frame;

    std::atomic<int> errorItemCount{0};
    std::deque<std::tuple<std::string, std::string, SurgeStorage::ErrorType>> errorItems;
    std::mutex errorItemsMutex;
    void onSurgeError(const std::string &msg, const std::string &title,
                      const SurgeStorage::ErrorType &type) override;

    std::atomic<bool> audioLatencyNotified{false};

    static int start_paramtag_value;

    void idle();
    int slowIdleCounter{0};
    bool queue_refresh;
    virtual void toggle_mod_editing();

    void forceLFODisplayRebuild();

    static long applyParameterOffset(long index);
    static long unapplyParameterOffset(long index);

    bool open(void *parent);
    void close();

    bool pause_idle_updates = false;
    int enqueuePatchId = -1;
    void flushEnqueuedPatchId()
    {
        auto t = enqueuePatchId;
        enqueuePatchId = -1;
        if (t >= 0)
        {
            synth->patchid_queue = t;
            // Looks scary but remember this only runs if audio thread is off
            synth->processAudioThreadOpsWhenAudioEngineUnavailable();
            patchCountdown = 200;
        }
    }

  public:
    typedef sst::plugininfra::KeyMapManager<Surge::GUI::KeyboardActions, Surge::GUI::n_kbdActions,
                                            juce::KeyPress>
        keymap_t;
    std::unique_ptr<keymap_t> keyMapManager;

    void setupKeymapManager();
    bool keyPressed(const juce::KeyPress &key, juce::Component *originatingComponent) override;
    std::string showShortcutDescription(const std::string &shortcutDesc,
                                        const std::string &shortcutDescMac);
    std::string showShortcutDescription(const std::string &shortcutDesc);

    bool debugFocus{false};
    void globalFocusChanged(juce::Component *fc) override;
#if SURGE_INCLUDE_MELATONIN_INSPECTOR
    std::unique_ptr<melatonin::Inspector> melatoninInspector;
#endif

  protected:
    virtual void setParameter(long index, float value);
    // listener class
    void valueChanged(Surge::GUI::IComponentTagValue *control) override;

    int32_t controlModifierClicked(Surge::GUI::IComponentTagValue *control,
                                   const juce::ModifierKeys &button,
                                   bool isDoubleClickEvent) override;
    void controlBeginEdit(Surge::GUI::IComponentTagValue *control) override;
    void controlEndEdit(Surge::GUI::IComponentTagValue *control) override;

    enum LearnMode
    {
        param_cc = 0,
        macro_cc,
        param_note
    };

    void createMIDILearnMenuEntries(juce::PopupMenu &parentMenu, const LearnMode learn_mode,
                                    const int ccid, Surge::GUI::IComponentTagValue *control);

    void changeSelectedOsc(int value);
    void changeSelectedScene(int value);

    void refreshSkin();

    bool showNoProcessingOverlay{false}; // this basically means it never shows

  public:
    void clearNoProcessingOverlay();

  public:
    // to make file chooser async it has to stick around
    std::unique_ptr<juce::FileChooser> fileChooser;

    void refresh_mod();
    void broadcastPluginAutomationChangeFor(Parameter *p);

    void effectSettingsBackgroundClick(int whichScene, Surge::Widgets::EffectChooser *c);

    void setDisabledForParameter(Parameter *p, Surge::Widgets::ModulatableControlInterface *s);
    void showSettingsMenu(const juce::Point<int> &where,
                          Surge::GUI::IComponentTagValue *launchFrom);

    // If n_scenes > 2, then this initialization and the modsource_editor one below will need to
    // adjust
    int current_scene = 0, current_osc[n_scenes] = {0, 0}, current_fx = 0;

    int getCurrentScene() { return current_scene; }
    int getCurrentLFOIDInEditor()
    {
        auto lfo_id = modsource_editor[current_scene] - ms_lfo1;
        return lfo_id;
    }

    /*
     * This is the hack to deal with the send return stacking
     */
    int whichSendActive[2]{0, 1}; // 13 and 24
    int whichReturnActive[2]{0, 1};
    bool isAHiddenSendOrReturn(Parameter *p);
    bool hasASendOrReturnToChangeTo(int pid);
    std::string nameOfStandardReturnToChangeTo(int pid);
    void activateFromCurrentFx();

    uint64_t lastObservedMidiNoteEventCount{0};

    modsources getSelectedModsource() { return modsource; }
    void setModsourceSelected(modsources ms, int ms_idx = 0);

    SurgeSynthesizer *synth = nullptr;

    void forceLfoDisplayRepaint();

  private:
    void openOrRecreateEditor();
    std::unique_ptr<Surge::Overlays::OverlayComponent> makeStorePatchDialog();
    void close_editor();
    bool isControlVisible(ControlGroup controlGroup, int controlGroupEntry);
    void repushAutomationFor(Parameter *p);
    bool editor_open = false;
    bool mod_editor = false;
    modsources modsource = ms_lfo1, modsource_editor[n_scenes] = {ms_lfo1, ms_lfo1};
    int modsource_index{0};
    int modsource_index_cache[n_scenes][n_lfos];
    int fxbypass_tag = 0, f1subtypetag = 0, f2subtypetag = 0, filterblock_tag = 0, fmconfig_tag = 0;
    double lastTempo = 0;
    int lastTSNum = 0, lastTSDen = 0;
    int lastOverlayRefresh = 0;
    void adjustSize(float &width, float &height) const;

    void update_deform_type(Parameter *p, int type);
    // Only updates a single bitfield. Does not check values.
    void update_deform_type_bit(Parameter *p, int type, int bit);

    struct patchdata
    {
        std::string name;
        std::string category;
        std::string comments;
        std::string author;
    };

    void setBitmapZoomFactor(float zf);
    void showTooLargeZoomError(double width, double height, float zf) const;

    /*
    ** Zoom Implementation
    **
    ** Zoom works by the system maintaining a zoom factor (created by user actions)
    **
    ** All zoom factors are set in units of percentages as ints. So no zoom is "100",
    ** and double size is "200"
    */

    float zoomFactor = 100;
    float initialZoomFactor = 100;

    int patchCountdown = -1;

  public:
    void populateDawExtraState(SurgeSynthesizer *synth);
    void loadFromDawExtraState(SurgeSynthesizer *synth);

    void setZoomCallback(std::function<void(SurgeGUIEditor *, bool resizeWindow)> f)
    {
        zoom_callback = f;
        setZoomFactor(getZoomFactor()); // notify the new callback
    }
    float getZoomFactor() const { return zoomFactor; }
    void setZoomFactor(float zf);
    void setZoomFactor(float zf, bool resizeWindow);
    void resizeWindow(float zf);
    void moveTopLeftTo(float tx, float ty);
    bool doesZoomFitToScreen(float zf,
                             float &correctedZf); // returns true if it fits; false if not; sets
                                                  // correctedZF to right size in either case

    void enqueueFXChainClear(int fxchain);
    void swapFX(int source, int target, SurgeSynthesizer::FXReorderMode m);

    /*
    ** Callbacks from the Status Panel. If this gets to be too many perhaps make these an interface?
    */
    void showMPEMenu(const juce::Point<int> &where, Surge::GUI::IComponentTagValue *launchFrom);
    void showTuningMenu(const juce::Point<int> &where, Surge::GUI::IComponentTagValue *launchFrom);
    void showZoomMenu(const juce::Point<int> &where, Surge::GUI::IComponentTagValue *launchFrom);
    void showLfoMenu(const juce::Point<int> &where, Surge::GUI::IComponentTagValue *launchFrom);

    void addEnvTrigOptions(juce::PopupMenu &, int);

    juce::PopupMenu::Options popupMenuOptions(const juce::Point<int> &where);
    juce::PopupMenu::Options popupMenuOptions(const juce::Component *c = nullptr,
                                              bool useComponentBounds = true);

    void showHTML(const std::string &s);

    void toggleMPE();
    void toggleTuning();
    void scaleFileDropped(const std::string &fn);
    void mappingFileDropped(const std::string &fn);
    std::string tuningToHtml();
    void tuningChanged();

    Surge::Widgets::ModulatableControlInterface *modSourceDragOverTarget{nullptr};
    Surge::Widgets::ModulatableControlInterface::ModulationState priorModulationState;
    bool modSourceButtonDraggedOver(Surge::Widgets::ModulationSourceButton *msb,
                                    const juce::Point<int> &); // true if over a droppable target
    void modSourceButtonDroppedAt(Surge::Widgets::ModulationSourceButton *msb,
                                  const juce::Point<int> &);
    void swapControllers(int t1, int t2);
    void openModTypeinOnDrop(modsources ms, Surge::Widgets::ModulatableControlInterface *sl,
                             int tgt, int modidx);

    void queueRebuildUI()
    {
        queue_refresh = true;
        synth->refresh_editor = true;
    }

    std::string midiMappingToHtml();
    std::string patchToHtml(bool includeDefaults = false);

    // These are unused right now
    enum SkinInspectorFlags
    {
        COLORS = 1 << 0,
        COMPONENTS = 1 << 1,

        ALL = COLORS | COMPONENTS
    };
    std::string skinInspectorHtml(SkinInspectorFlags f = ALL);

    /*
    ** Modulation Hover Support
    */
    void sliderHoverStart(int tag);
    void sliderHoverEnd(int tag);

    int getWindowSizeX() const { return wsx; }
    int getWindowSizeY() const { return wsy; }

    void repaintFrame();
    /*
     * We have an enumerated set of overlay tags which we can push
     * to the UI. You *have* to give a new overlay type a tag in
     * order for it to work.
     */
    enum OverlayTags
    {
        NO_EDITOR,
        MSEG_EDITOR,
        SAVE_PATCH,
        PATCH_BROWSER,
        MODULATION_EDITOR,
        FORMULA_EDITOR,
        WT_EDITOR,
        TUNING_EDITOR,
        WAVESHAPER_ANALYZER,
        FILTER_ANALYZER,
        OSCILLOSCOPE,
        KEYBINDINGS_EDITOR,
        ACTION_HISTORY,
        OPEN_SOUND_CONTROL_SETTINGS,

        n_overlay_tags,
    };

  private:
    std::unique_ptr<Surge::Overlays::OverlayComponent> createOverlay(OverlayTags);

  public:
    void showOverlay(OverlayTags olt)
    {
        showOverlay(olt, [](auto *s) {});
    }
    void showOverlay(OverlayTags olt,
                     std::function<void(Surge::Overlays::OverlayComponent *)> setup);
    void closeOverlay(OverlayTags);
    void toggleOverlay(OverlayTags t)
    {
        if (isAnyOverlayPresent(t))
        {
            closeOverlay(t);
        }
        else
        {
            showOverlay(t);
        }
    }
    bool overlayConsumesKeyboard(OverlayTags);
    // I will be handed a pointer I need to keep around you know.
    Surge::Overlays::OverlayWrapper *addJuceEditorOverlay(
        std::unique_ptr<juce::Component> c,
        std::string editorTitle, // A window display title - whatever you want
        OverlayTags editorTag,   // A tag by editor class. Please unique, no spaces.
        const juce::Rectangle<int> &containerBounds, bool showCloseButton = true,
        std::function<void()> onClose = []() {}, bool forceModal = false);
    std::unordered_map<OverlayTags, std::unique_ptr<Surge::Overlays::OverlayWrapper>> juceOverlays;
    std::vector<std::unique_ptr<Surge::Overlays::OverlayWrapper>> juceDeleteOnIdle;
    std::unordered_map<OverlayTags, bool> isTornOut;
    void rezoomOverlays();
    void frontNonModalOverlays();

    void dismissEditorOfType(OverlayTags ofType);
    bool isAnyOverlayPresent(OverlayTags tag)
    {
        if (juceOverlays.find(tag) != juceOverlays.end() && juceOverlays[tag])
            return true;

        return false;
    }
    bool isAnyOverlayOpenAtAll() { return !juceOverlays.empty(); }
    bool isPointWithinOverlay(const juce::Point<int> point);
    juce::Component *getOverlayIfOpen(OverlayTags tag);
    template <typename T> T *getOverlayIfOpenAs(OverlayTags tag)
    {
        return dynamic_cast<T *>(getOverlayIfOpen(tag));
    }

    Surge::Overlays::OverlayWrapper *getOverlayWrapperIfOpen(OverlayTags tag);
    void refreshOverlayWithOpenClose(OverlayTags tag)
    {
        refreshAndMorphOverlayWithOpenClose(tag, tag);
    }
    void refreshAndMorphOverlayWithOpenClose(OverlayTags tag, OverlayTags newTag);
    bool updateOverlayContentIfPresent(OverlayTags tag);

    void updateWaveshaperOverlay(); // this is the only overlay which updates from patch values

    std::string getDisplayForTag(long tag, bool external = false, float value = 0);
    float getF01FromString(long tag, const std::string &s);
    float getModulationF01FromString(long tag, const std::string &s);
    std::string getAccessibleModulationVoiceover(long tag);

    void queuePatchFileLoad(const std::string &file)
    {
        {
            std::lock_guard<std::mutex> mg(synth->patchLoadSpawnMutex);
            undoManager()->pushPatch();
            strncpy(synth->patchid_file, file.c_str(), FILENAME_MAX);
            synth->has_patchid_file = true;
        }
        synth->processAudioThreadOpsWhenAudioEngineUnavailable();
    }

    void loadPatchWithDirtyCheck(bool increment, bool isCategory, bool insideCategory = false);

    void openMacroRenameDialog(const int ccid, const juce::Point<int> where,
                               Surge::Widgets::ModulationSourceButton *msb);
    void openLFORenameDialog(const int lfo_id, const juce::Point<int> where, juce::Component *r);

    void lfoShapeChanged(int prior, int curr);
    void broadcastMSEGState();
    int msegIsOpenFor = -1, msegIsOpenInScene = -1;
    bool showMSEGEditorOnNextIdleOrOpen = false;

    std::vector<DAWExtraStateStorage::EditorState::OverlayState> overlaysForNextIdle;

    std::deque<std::pair<std::string, int>> accAnnounceStrings;
    void enqueueAccessibleAnnouncement(const std::string &s);
    void announceGuiState();
    void setAccessibilityInformationByParameter(juce::Component *c, Parameter *p,
                                                const std::string &action);

    static void setAccessibilityInformationByTitleAndAction(juce::Component *c,
                                                            const std::string &title,
                                                            const std::string &action);

    Surge::Overlays::MSEGEditor::State msegEditState[n_scenes][n_lfos];
    Surge::Overlays::MSEGEditor::State mostRecentCopiedMSEGState;

    int oscilatorMenuIndex[n_scenes][n_oscs] = {0};

  public:
    bool canDropTarget(const std::string &fname); // these come as const char* from vstgui
    bool onDrop(const std::string &fname);

    const std::unique_ptr<Surge::GUI::UndoManager> &undoManager();
    void setParamFromUndo(int paramId, pdata val);
    void pushParamToUndoRedo(int paramId, Surge::GUI::UndoManager::Target which);
    void applyToParamForUndo(int paramId, std::function<void(Parameter *)> f);
    void setModulationFromUndo(int paramId, modsources ms, int scene, int idx, float val,
                               bool muted);
    void pushModulationToUndoRedo(int paramId, modsources ms, int scene, int idx,
                                  Surge::GUI::UndoManager::Target which);
    void setStepSequencerFromUndo(int scene, int lfoid, const StepSequencerStorage &val);
    void setMSEGFromUndo(int scene, int lfoid, const MSEGStorage &val);
    void setFormulaFromUndo(int scene, int lfoid, const FormulaModulatorStorage &val);
    void setLFONameFromUndo(int scene, int lfoid, int index, const std::string &n);
    void setMacroNameFromUndo(int ccid, const std::string &n);
    void setMacroValueFromUndo(int ccid, float val);
    void setTuningFromUndo(const Tunings::Tuning &);
    const Tunings::Tuning &getTuningForRedo();
    const fs::path pathForCurrentPatch();
    void ensureParameterItemIsFocused(Parameter *p);
    void setPatchFromUndo(void *data, size_t datasz);

    void playNote(char key, char vel);
    void releaseNote(char key, char vel);

  private:
    juce::Rectangle<int> positionForModulationGrid(modsources entry);
    juce::Rectangle<int> positionForModOverview();
    std::unique_ptr<Surge::Widgets::ModulationOverviewLaunchButton> modOverviewLauncher;

    int wsx = BASE_WINDOW_SIZE_X;
    int wsy = BASE_WINDOW_SIZE_Y;

    /**
     * findLargestFittingZoomBetween
     *
     * Finds the largest zoom which will fit your current screen between a lower and upper bound.
     * Will never return something smaller than lower or larger than upper. Default is as large as
     * possible, quantized in units of zoomQuanta, with the maximum screen usage percentages
     * protecting for screen real estate. The function also needs to know the 100% size of the UI
     * the baseW and baseH)
     *
     * for instance findLargestFittingZoomBetween( 100, 200, 5, 90, bw, bh )
     *
     * would find the largest possible zoom which uses at most 90% of your screen real estate but
     * would also guarantee that the result % 5 == 0.
     */
    int findLargestFittingZoomBetween(int zoomLow, int zoomHigh, int zoomQuanta,
                                      int percentageOfScreenAvailable, float baseW, float baseH);

  public:
    void showAboutScreen(int devModeGrid = -1);
    void hideAboutScreen();

    void showMidiLearnOverlay(const juce::Rectangle<int> &r);
    void hideMidiLearnOverlay();

  private:
    std::function<void(SurgeGUIEditor *, bool resizeWindow)> zoom_callback;
    bool zoomInvalid = false;
    static constexpr int minimumZoom = 25;

    int selectedFX[n_fx_slots];
    std::string fxPresetName[n_fx_slots];

    int processRunningCheckEvery{0};
    std::unique_ptr<juce::Component> noProcessingOverlay{nullptr};

  public:
    std::shared_ptr<SurgeImageStore> bitmapStore = nullptr;
    // made public so that EffectChooser can get to it!
    std::unique_ptr<Surge::Widgets::FxMenu> fxMenu;

  private:
    std::array<std::unique_ptr<Surge::Widgets::VuMeter>, n_fx_slots + 1> vu;
    bool firstTimePatchLoad{true};
    std::unique_ptr<Surge::Widgets::PatchSelector> patchSelector;
    std::unique_ptr<Surge::Widgets::PatchSelectorCommentTooltip> patchSelectorComment;
    std::unique_ptr<Surge::Widgets::OscillatorMenu> oscMenu;
    std::unique_ptr<Surge::Widgets::WaveShaperSelector> waveshaperSelector;
    std::unique_ptr<Surge::Widgets::Switch> waveshaperAnalysis;

    /* Infowindow members and functions */
    std::unique_ptr<Surge::Widgets::ParameterInfowindow> paramInfowindow;

    void setupAlternates(modsources ms);

  public:
    void showPatchCommentTooltip(const std::string &comment);
    void hidePatchCommentTooltip();
    void showInfowindow(int ptag, juce::Rectangle<int> relativeTo, bool isModulated);
    void showInfowindowSelfDismiss(int ptag, juce::Rectangle<int> relativeTo, bool isModulated);
    void updateInfowindowContents(int ptag, bool isModulated);
    void hideInfowindowNow();
    void hideInfowindowSoon();
    void idleInfowindow();
    enum InfoQAction
    {
        START,
        CANCEL,
        LEAVE
    };
    void enqueueFutureInfowindow(int ptag, const juce::Rectangle<int> &around, InfoQAction action);
    enum InfoQState
    {
        NONE,
        COUNTDOWN,
        SHOWING,
        DISMISSED_BEFORE
    } infoQState{NONE};
    int infoQCountdown{-1};
    int infoQTag{-1};
    juce::Rectangle<int> infoQBounds;

    /*
     * Favorites support
     */
    void setPatchAsFavorite(const std::string &pname, bool b);
    bool isPatchFavorite();
    bool isPatchUser();

    void setSpecificPatchAsFavorite(int patchid, bool b);

    /*
     * Modulation Client API
     */
    struct SelfModulationGuard
    {
        SelfModulationGuard(SurgeGUIEditor *ed) : moded(ed) { moded->selfModulation = true; }
        ~SelfModulationGuard() { moded->selfModulation = false; }
        SurgeGUIEditor *moded;
    };
    std::atomic<bool> selfModulation{false}, needsModUpdate{false};
    void modSet(long ptag, modsources modsource, int modsourceScene, int index, float value,
                bool isNew) override;
    void modMuted(long ptag, modsources modsource, int modsourceScene, int index,
                  bool mute) override;
    void modCleared(long ptag, modsources modsource, int modsourceScene, int index) override;

    SurgePatch &getPatch() { return synth->storage.getPatch(); }
    SurgeStorage *getStorage() { return &(synth->storage); }

    std::unique_ptr<Surge::Widgets::EffectChooser> effectChooser;

  private:
    Surge::GUI::IComponentTagValue *statusMPE = nullptr, *statusTune = nullptr,
                                   *statusZoom = nullptr, *actionUndo = nullptr,
                                   *actionRedo = nullptr;
    std::unique_ptr<Surge::Overlays::AboutScreen> aboutScreen;

    std::unique_ptr<juce::Drawable> midiLearnOverlay;

    std::unique_ptr<Surge::Overlays::TypeinParamEditor> typeinParamEditor;
    bool setParameterFromString(Parameter *p, const std::string &s, std::string &errMsg);
    bool setParameterModulationFromString(Parameter *p, modsources ms, int modsourceScene,
                                          int modidx, const std::string &s, std::string &errMsg);
    bool setControlFromString(modsources ms, const std::string &s);
    void hideTypeinParamEditor();
    friend struct Surge::Overlays::TypeinParamEditor;
    friend struct Surge::Overlays::PatchStoreDialog;
    friend struct Surge::Widgets::MainFrame;

    Surge::GUI::IComponentTagValue *lfoEditSwitch = nullptr;
    int typeinResetCounter = -1;
    std::string typeinResetLabel = "";

    std::unique_ptr<Surge::Overlays::MiniEdit> miniEdit;

  public:
    void promptForMiniEdit(const std::string &value, const std::string &prompt,
                           const std::string &title, const juce::Point<int> &where,
                           std::function<void(const std::string &)> onOK,
                           juce::Component *returnFocusTo /* can be nullptr */);

    /*
     * Weak pointers to some logical items for focus return
     */
    juce::Component *mpeStatus{nullptr}, *zoomStatus{nullptr}, *tuneStatus{nullptr},
        *mainMenu{nullptr}, *lfoMenuButton{nullptr}, *undoButton{nullptr}, *redoButton{nullptr},
        *lfoRateSlider{nullptr};
    Surge::Widgets::ModulatableControlInterface *filterCutoffSlider[2]{nullptr, nullptr},
        *filterResonanceSlider[2]{nullptr, nullptr};
    /*
     * This is the JUCE component management
     */
    std::array<std::unique_ptr<Surge::Widgets::EffectLabel>, 15> effectLabels;

    bool scanJuceSkinComponents{false};
    std::unordered_map<Surge::GUI::Skin::Control::sessionid_t, std::unique_ptr<juce::Component>>
        juceSkinComponents;
    std::unordered_map<Surge::GUI::Skin::Control::sessionid_t, juce::Component *>
        juceSkinComponentsWeak;

    /*
     * This form of the component creator assumes that after you get the unique ptr you
     * will externally stow it away in juceSkinComponents with a move later and the ownership
     * pattern comes through the collective array
     */
    template <typename T>
    std::unique_ptr<T> componentForSkinSession(const Surge::GUI::Skin::Control::sessionid_t id)
    {
        std::unique_ptr<T> hsw;
        if (juceSkinComponents.find(id) != juceSkinComponents.end())
        {
            // this has the wrong type for a std::move so do this instead - sigh
            auto pq = dynamic_cast<T *>(juceSkinComponents[id].get());
            if (pq)
            {
                juceSkinComponents[id].release();
                juceSkinComponents.erase(id);
                hsw = std::unique_ptr<T>{pq};
            }
            else
            {
                // This happens if a control switches types - like, say,
                // a slider goes to a menu so delete the old one
                juceSkinComponents.erase(id);
                hsw = std::make_unique<T>();
            }
        }
        else
        {
            hsw = std::make_unique<T>();
        }
        return std::move(hsw);
    }

    /*
     * This form of the component creator assumes that you hold the unique in a named
     * member and don't cache it in the shared array, but we maintain a weak reference array
     * to make sure the value we expect is still the correct one.
     */
    template <typename T>
    void componentForSkinSessionOwnedByMember(const Surge::GUI::Skin::Control::sessionid_t id,
                                              std::unique_ptr<T> &res)
    {
        if (juceSkinComponentsWeak.find(id) != juceSkinComponentsWeak.end())
        {
            if (juceSkinComponentsWeak[id] == res.get())
            {
                return;
            }
        }
        res = std::make_unique<T>();
        juceSkinComponentsWeak[id] = res.get();
    }

  private:
    std::unique_ptr<Surge::Widgets::VerticalLabel> lfoNameLabel;
    std::unique_ptr<juce::Label> fxPresetLabel;

  public:
  private:
    Parameter *typeinEditTarget = nullptr;
    int typeinModSource = -1;

  public:
    std::unique_ptr<Surge::Widgets::OscillatorWaveformDisplay> oscWaveform;

  private:
    std::unique_ptr<Surge::Widgets::NumberField> polydisp;
    std::unique_ptr<Surge::Widgets::NumberField> splitpointControl;

    static const int n_paramslots = 1024;
    Surge::Widgets::ModulatableControlInterface *param[n_paramslots] = {};
    Surge::GUI::IComponentTagValue *nonmod_param[n_paramslots] = {};
    std::array<std::unique_ptr<Surge::Widgets::ModulationSourceButton>, n_modsources> gui_modsrc;
    std::unique_ptr<Surge::Widgets::LFOAndStepDisplay> lfoDisplay;
    int lfoDisplayRepaintCountdown{0};

    Surge::Widgets::Switch *filtersubtype[2] = {};

  public:
    bool useDevMenu = false;
    SurgeSynthEditor *juceEditor{nullptr};

  private:
    float blinktimer = 0;
    bool blinkstate = false;
    int firstIdleCountdown = 0;
    int firstErrorIdleCountdown{30};
    int sendStructureChangeIn = -1;

    juce::PopupMenu makeSmoothMenu(const juce::Point<int> &where,
                                   const Surge::Storage::DefaultKey &key, int defaultValue,
                                   std::function<void(Modulator::SmoothingMode)> setSmooth);

    void makeMpeTimbreMenu(juce::PopupMenu &menu, const bool asSubMenu);
    juce::PopupMenu makeMpeMenu(const juce::Point<int> &rect, bool showhelp);
    juce::PopupMenu makeTuningMenu(const juce::Point<int> &rect, bool showhelp);
    juce::PopupMenu makeZoomMenu(const juce::Point<int> &rect, bool showhelp);
    juce::PopupMenu makeSkinMenu(const juce::Point<int> &rect);
    juce::PopupMenu makeMouseBehaviorMenu(const juce::Point<int> &rect);
    juce::PopupMenu makePatchDefaultsMenu(const juce::Point<int> &rect);
    juce::PopupMenu makeValueDisplaysMenu(const juce::Point<int> &rect);
    juce::PopupMenu makeWorkflowMenu(const juce::Point<int> &rect);
    juce::PopupMenu makeAccesibilityMenu(const juce::Point<int> &rect);
    juce::PopupMenu makeDataMenu(const juce::Point<int> &rect);
    juce::PopupMenu makeMidiMenu(const juce::Point<int> &rect);
    juce::PopupMenu makeDevMenu(const juce::Point<int> &rect);
    juce::PopupMenu makeLfoMenu(const juce::Point<int> &rect);
    juce::PopupMenu makeMonoModeOptionsMenu(const juce::Point<int> &rect, bool updateDefaults);
    juce::PopupMenu makeOSCMenu(const juce::Point<int> &where);

    void makeScopeEntry(juce::PopupMenu &menu);

    void setRecommendedAccessibility();

  public:
    void addHelpHeaderTo(const std::string &lab, const std::string &hu, juce::PopupMenu &m) const;

  public:
    bool getShowVirtualKeyboard();
    void setShowVirtualKeyboard(bool b);
    void toggleVirtualKeyboard();

    int vkbForward{0};
    bool shouldForwardKeysToVKB() { return vkbForward == 0; }

    bool getUseKeyboardShortcuts();
    void setUseKeyboardShortcuts(bool b);
    void toggleUseKeyboardShortcuts();

    enum AlertButtonStyle
    {
        OK_CANCEL,
        YES_NO,
        OK
    };

    void alertBox(const std::string &title, const std::string &prompt, std::function<void()> onOk,
                  std::function<void()> onCancel, AlertButtonStyle buttonStyle);

    void alertOKCancel(const std::string &title, const std::string &prompt,
                       std::function<void()> onOk, std::function<void()> onCancel = nullptr)
    {
        alertBox(title, prompt, onOk, onCancel, AlertButtonStyle::OK_CANCEL);
    }
    void alertYesNo(const std::string &title, const std::string &prompt, std::function<void()> onOk,
                    std::function<void()> onCancel = nullptr)
    {
        alertBox(title, prompt, onOk, onCancel, AlertButtonStyle::YES_NO);
    }
    void messageBox(const std::string &title, const std::string &prompt)
    {
        alertBox(title, prompt, nullptr, nullptr, AlertButtonStyle::OK);
    }

    std::unique_ptr<Surge::Overlays::Alert> alert;

    enum AskAgainStates
    {
        DUNNO = 1,
        ALWAYS = 10,
        NEVER = 100
    };

    // Return whether we called the OK action automatically or not
    bool promptForOKCancelWithDontAskAgain(const ::std::string &title, const std::string &msg,
                                           Surge::Storage::DefaultKey dontAskAgainKey,
                                           std::function<void()> okCallback,
                                           std::string ynMessage = "Don't ask me again",
                                           AskAgainStates askAgainDefault = DUNNO);

  private:
    bool scannedForMidiPresets = false;

    void resetSmoothing(Modulator::SmoothingMode t);
    void resetPitchSmoothing(Modulator::SmoothingMode t);

  public:
    std::string helpURLFor(Parameter *p); // this requires internal state so doesn't have statics
    std::string
    helpURLForSpecial(const std::string &special); // these can be either this way or static

    static std::string helpURLForSpecial(SurgeStorage *storage, const std::string &special);
    static std::string fullyResolvedHelpURL(const std::string &helpurl);

  private:
    /*
     * the add remove handlers
     */
    friend class Surge::Widgets::MainFrame;
    std::unordered_map<juce::Component *, juce::Component *> containedComponents;
    void addComponentWithTracking(juce::Component *target, juce::Component &source);
    void addAndMakeVisibleWithTracking(juce::Component *target, juce::Component &source);
    void addAndMakeVisibleWithTrackingInCG(ControlGroup cg, juce::Component &source);

    void resetComponentTracking();
    void removeUnusedTrackedComponents();

  public:
    void promptForUserValueEntry(Parameter *p, juce::Component *c, int modsource, int modScene,
                                 int modindex);
    bool promptForUserValueEntry(Surge::Widgets::ModulatableControlInterface *mci);
    bool promptForUserValueEntry(uint32_t tag, juce::Component *c);
    void promptForUserValueEntry(Parameter *p, juce::Component *c)
    {
        promptForUserValueEntry(p, c, -1, -1, -1);
    }

    /*
    ** Skin support
    */
  public:
    Surge::GUI::Skin::ptr_t currentSkin;

  private:
    void setupSkinFromEntry(const Surge::GUI::SkinDB::Entry &entry);
    void reloadFromSkin();
    Surge::GUI::IComponentTagValue *
    layoutComponentForSkin(std::shared_ptr<Surge::GUI::Skin::Control> skinCtrl, long tag,
                           int paramIndex = -1, Parameter *p = nullptr, int style = 0);

    /*
    ** General MIDI CC names
    */
    const std::string midicc_names[128] = {"Bank Select MSB",
                                           "Modulation Wheel MSB",
                                           "Breath Controller MSB",
                                           "Control 3 MSB",
                                           "Foot Controller MSB",
                                           "Portamento Time MSB",
                                           "Data Entry MSB",
                                           "Volume MSB",
                                           "Balance MSB",
                                           "Control 9 MSB",
                                           "Pan MSB",
                                           "Expression MSB",
                                           "Effect #1 MSB",
                                           "Effect #2 MSB",
                                           "Control 14 MSB",
                                           "Control 15 MSB",
                                           "General Purpose Controller #1 MSB",
                                           "General Purpose Controller #2 MSB",
                                           "General Purpose Controller #3 MSB",
                                           "General Purpose Controller #4 MSB",
                                           "Control 20 MSB",
                                           "Control 21 MSB",
                                           "Control 22 MSB",
                                           "Control 23 MSB",
                                           "Control 24 MSB",
                                           "Control 25 MSB",
                                           "Control 26 MSB",
                                           "Control 27 MSB",
                                           "Control 28 MSB",
                                           "Control 29 MSB",
                                           "Control 30 MSB",
                                           "Control 31 MSB",
                                           "Bank Select LSB",
                                           "Modulation Wheel LSB",
                                           "Breath Controller LSB",
                                           "Control 3 LSB",
                                           "Foot Controller LSB",
                                           "Portamento Time LSB",
                                           "Data Entry LSB",
                                           "Volume LSB",
                                           "Balance LSB",
                                           "Control 9 LSB",
                                           "Pan LSB",
                                           "Expression LSB",
                                           "Effect #1 LSB",
                                           "Effect #2 LSB",
                                           "Control 14 LSB",
                                           "Control 15 LSB",
                                           "General Purpose Controller #1 LSB",
                                           "General Purpose Controller #2 LSB",
                                           "General Purpose Controller #3 LSB",
                                           "General Purpose Controller #4 LSB",
                                           "Control 20 LSB",
                                           "Control 21 LSB",
                                           "Control 22 LSB",
                                           "Control 23 LSB",
                                           "Control 24 LSB",
                                           "Control 25 LSB",
                                           "Control 26 LSB",
                                           "Control 27 LSB",
                                           "Control 28 LSB",
                                           "Control 29 LSB",
                                           "Control 30 LSB",
                                           "Control 31 LSB",
                                           "Sustain Pedal",
                                           "Portamento Pedal",
                                           "Sostenuto Pedal",
                                           "Soft Pedal",
                                           "Legato Pedal",
                                           "Hold Pedal",
                                           "Sound Control #1 Sound Variation",
                                           "Sound Control #2 Timbre",
                                           "Sound Control #3 Release Time",
                                           "Sound Control #4 Attack Time",
                                           "Sound Control #5 Brightness / MPE Timbre",
                                           "Sound Control #6 Decay Time",
                                           "Sound Control #7 Vibrato Rate",
                                           "Sound Control #8 Vibrato Depth",
                                           "Sound Control #9 Vibrato Delay",
                                           "Sound Control #10 Control 79",
                                           "General Purpose Controller #5",
                                           "General Purpose Controller #6",
                                           "General Purpose Controller #7",
                                           "General Purpose Controller #8",
                                           "Portamento Control",
                                           "Control 85",
                                           "Control 86",
                                           "Control 87",
                                           "High Resolution Velocity Prefix",
                                           "Control 89",
                                           "Control 90",
                                           "Reverb Send Level",
                                           "Tremolo Depth",
                                           "Chorus Send Level",
                                           "Celeste Depth",
                                           "Phaser Depth",
                                           "Data Increment",
                                           "Data Decrement",
                                           "NRPN LSB",
                                           "NRPN MSB",
                                           "RPN LSB",
                                           "RPN MLSB",
                                           "Control 102",
                                           "Control 103",
                                           "Control 104",
                                           "Control 105",
                                           "Control 106",
                                           "Control 107",
                                           "Control 108",
                                           "Control 109",
                                           "Control 110",
                                           "Control 111",
                                           "Control 112",
                                           "Control 113",
                                           "Control 114",
                                           "Control 115",
                                           "Control 116",
                                           "Control 117",
                                           "Control 118",
                                           "Control 119",
                                           "Control 120 All Sound Off",
                                           "Control 121 Reset All Controllers",
                                           "Control 122 Local Control On/Off",
                                           "Control 123 All Notes Off",
                                           "Control 124 Omni Mode Off",
                                           "Control 125 Omni Mode On",
                                           "Control 126 Mono Mode Off",
                                           "Control 127 Mono Mode On"};
};

#endif // SURGE_SRC_SURGE_XT_GUI_SURGEGUIEDITOR_H
