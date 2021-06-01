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

#pragma once

#include "globals.h"

#include <JuceHeader.h>

#include "efvg/escape_from_vstgui.h"
#include "SurgeStorage.h"
#include "SurgeBitmaps.h"

#include "SurgeSynthesizer.h"

#include "SkinSupport.h"
#include "SkinColors.h"

#include "overlays/MSEGEditor.h"
#include "widgets/ModulatableControlInterface.h"

#include <vector>
#include <thread>
#include <atomic>

typedef EscapeFromVSTGUI::JuceVSTGUIEditorAdapter EditorType;
class SurgeSynthEditor;

class CModulationSourceButton;
class CAboutBox;

namespace Surge
{
namespace Widgets
{
struct AboutScreen;
struct EffectChooser;
struct EffectLabel;
struct ModulatableControlInterface;
struct ParameterInfowindow;
struct PatchSelector;
struct Switch;
struct VerticalLabel;
struct VuMeter;
} // namespace Widgets
} // namespace Surge

struct SGEDropAdapter;

class SurgeGUIEditor : public EditorType,
                       public VSTGUI::IControlListener,
                       public VSTGUI::IKeyboardHook,
                       public SurgeStorage::ErrorListener
{
  private:
    using super = EditorType;

  public:
    SurgeGUIEditor(SurgeSynthEditor *juceEditor, SurgeSynthesizer *synth);
    virtual ~SurgeGUIEditor();

    std::atomic<int> errorItemCount{0};
    std::vector<std::pair<std::string, std::string>> errorItems;
    std::mutex errorItemsMutex;
    void onSurgeError(const std::string &msg, const std::string &title) override;

    static int start_paramtag_value;

    void idle();
    bool queue_refresh;
    virtual void toggle_mod_editing();

    virtual void beginEdit(long index);
    virtual void endEdit(long index);

    static long applyParameterOffset(long index);
    static long unapplyParameterOffset(long index);

    bool open(void *parent) override;
    void close() override;

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
            synth->processThreadunsafeOperations();
            patchCountdown = 200;
        }
    }

  protected:
    int32_t onKeyDown(const VstKeyCode &code,
                      VSTGUI::CFrame *frame) override; ///< should return 1 if no further key down
                                                       ///< processing should apply, otherwise -1
    int32_t onKeyUp(const VstKeyCode &code,
                    VSTGUI::CFrame *frame) override; ///< should return 1 if no further key up
                                                     ///< processing should apply, otherwise -1

    virtual void setParameter(long index, float value);

    // listener class
    void valueChanged(VSTGUI::CControlValueInterface *control) override;
    int32_t controlModifierClicked(VSTGUI::CControlValueInterface *pControl,
                                   VSTGUI::CButtonState button) override;
    void controlBeginEdit(VSTGUI::CControlValueInterface *pControl) override;
    void controlEndEdit(VSTGUI::CControlValueInterface *pControl) override;

  public:
    void refresh_mod();
    void forceautomationchangefor(Parameter *p);

    void effectSettingsBackgroundClick(int whichScene);

    void setDisabledForParameter(Parameter *p, Surge::Widgets::ModulatableControlInterface *s);
    void showSettingsMenu(VSTGUI::CRect &menuRect);

    static bool fromSynthGUITag(SurgeSynthesizer *synth, int tag, SurgeSynthesizer::ID &q);
    // If n_scenes > 2, then this initialization and the modsource_editor one below will need to
    // adjust
    int current_scene = 0, current_osc[n_scenes] = {0, 0}, current_fx = 0;

  private:
    void openOrRecreateEditor();
    void makeStorePatchDialog();
    void close_editor();
    bool isControlVisible(ControlGroup controlGroup, int controlGroupEntry);
    void repushAutomationFor(Parameter *p);
    SurgeSynthesizer *synth = nullptr;
    bool editor_open = false;
    bool mod_editor = false;
    modsources modsource = ms_lfo1, modsource_editor[n_scenes] = {ms_lfo1, ms_lfo1};
    int fxbypass_tag = 0, f1subtypetag = 0, f2subtypetag = 0, filterblock_tag = 0, fmconfig_tag = 0;
    double lastTempo = 0;
    int lastTSNum = 0, lastTSDen = 0;
    void draw_infowindow(int ptag, VSTGUI::BaseViewFunctions *control, bool modulate,
                         bool forceMB = false);
    void adjustSize(float &width, float &height) const;

    struct patchdata
    {
        std::string name;
        std::string category;
        std::string comments;
        std::string author;
    };

    void setBitmapZoomFactor(float zf);
    void showTooLargeZoomError(double width, double height, float zf) const;
    void showMinimumZoomError() const;

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
    void populateDawExtraState(SurgeSynthesizer *synth)
    {
        auto des = &(synth->storage.getPatch().dawExtraState);

        des->isPopulated = true;
        des->editor.instanceZoomFactor = zoomFactor;
        des->editor.current_scene = current_scene;
        des->editor.current_fx = current_fx;
        des->editor.modsource = modsource;
        for (int i = 0; i < n_scenes; ++i)
        {
            des->editor.current_osc[i] = current_osc[i];
            des->editor.modsource_editor[i] = modsource_editor[i];

            des->editor.msegStateIsPopulated = true;
            for (int lf = 0; lf < n_lfos; ++lf)
            {
                des->editor.msegEditState[i][lf].timeEditMode = msegEditState[i][lf].timeEditMode;
            }
        }
        des->editor.isMSEGOpen = (editorOverlayTagAtClose == "msegEditor");
    }

    void loadFromDAWExtraState(SurgeSynthesizer *synth)
    {
        auto des = &(synth->storage.getPatch().dawExtraState);
        if (des->isPopulated)
        {
            auto sz = des->editor.instanceZoomFactor;
            if (sz > 0)
                setZoomFactor(sz);
            current_scene = des->editor.current_scene;
            current_fx = des->editor.current_fx;
            modsource = des->editor.modsource;

            for (int i = 0; i < n_scenes; ++i)
            {
                current_osc[i] = des->editor.current_osc[i];
                modsource_editor[i] = des->editor.modsource_editor[i];
                if (des->editor.msegStateIsPopulated)
                {
                    for (int lf = 0; lf < n_lfos; ++lf)
                    {
                        msegEditState[i][lf].timeEditMode =
                            des->editor.msegEditState[i][lf].timeEditMode;
                    }
                }
            }
            if (des->editor.isMSEGOpen)
            {
                showMSEGEditorOnNextIdleOrOpen = true;
            }
        }
    }

    void setZoomCallback(std::function<void(SurgeGUIEditor *, bool resizeWindow)> f)
    {
        zoom_callback = f;
        setZoomFactor(getZoomFactor()); // notify the new callback
    }
    float getZoomFactor() const { return zoomFactor; }
    void setZoomFactor(float zf);
    void setZoomFactor(float zf, bool resizeWindow);
    void resizeWindow(float zf);
    bool doesZoomFitToScreen(float zf,
                             float &correctedZf); // returns true if it fits; false if not; sets
                                                  // correctedZF to right size in either case

    void swapFX(int source, int target, SurgeSynthesizer::FXReorderMode m);

    /*
    ** Callbacks from the Status Panel. If this gets to be too many perhaps make these an interface?
    */
    void showMPEMenu(VSTGUI::CPoint &where);
    void showTuningMenu(VSTGUI::CPoint &where);
    void showZoomMenu(VSTGUI::CPoint &where);
    void showLfoMenu(VSTGUI::CPoint &menuRect);

    void showHTML(const std::string &s);

    void toggleMPE();
    void toggleTuning();
    void scaleFileDropped(std::string fn);
    void mappingFileDropped(std::string fn);
    std::string tuningToHtml();

    void swapControllers(int t1, int t2);
    void openModTypeinOnDrop(int ms, Surge::Widgets::ModulatableControlInterface *sl, int tgt);

    void queueRebuildUI()
    {
        queue_refresh = true;
        synth->refresh_editor = true;
    }

    std::string midiMappingToHtml();

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

    void invalidateFrame() const { frame->invalid(); }
    /*
     * We have an enumerated set of overlay tags which we can push
     * to the UI. You *have* to give a new overlay type a tag in
     * order for it to work.
     */
    enum OverlayTags
    {
        NO_EDITOR,
        MSEG_EDITOR,
        STORE_PATCH,
        PATCH_BROWSER,
        MODULATION_EDITOR,
        FORMULA_EDITOR,
        WAVETABLESCRIPTING_EDITOR,
    };

    void addEditorOverlay(
        VSTGUI::CView *c,
        std::string editorTitle, // A window display title - whatever you want
        OverlayTags editorTag,   // A tag by editor class. Please unique, no spaces.
        const VSTGUI::CPoint &topleft = VSTGUI::CPoint(0, 0), bool modalOverlay = true,
        bool hasCloseButton = true, std::function<void()> onClose = []() {});
    void dismissEditorOfType(OverlayTags ofType);
    OverlayTags topmostEditorTag()
    {
        if (!editorOverlay.size())
            return NO_EDITOR;
        return editorOverlay.back().first;
    }
    bool isAnyOverlayPresent(OverlayTags tag)
    {
        for (auto el : editorOverlay)
        {
            if (el.first == tag)
                return true;
        }
        return false;
    }

    std::string getDisplayForTag(long tag);

    void queuePatchFileLoad(std::string file)
    {
        strncpy(synth->patchid_file, file.c_str(), FILENAME_MAX);
        synth->has_patchid_file = true;
    }

    void closeStorePatchDialog();
    void showStorePatchDialog();

    void closePatchBrowserDialog();
    void showPatchBrowserDialog();

    void closeModulationEditorDialog();
    void showModulationEditorDialog();

    void closeFormulaEditorDialog();
    void showFormulaEditorDialog();
    void toggleFormulaEditorDialog();

    void closeWavetableScripter();
    void showWavetableScripter();

    void lfoShapeChanged(int prior, int curr);
    void showMSEGEditor();
    void closeMSEGEditor();
    void toggleMSEGEditor();
    void broadcastMSEGState();
    int msegIsOpenFor = -1, msegIsOpenInScene = -1;
    bool showMSEGEditorOnNextIdleOrOpen = false;

    MSEGEditor::State msegEditState[n_scenes][n_lfos];
    MSEGEditor::State mostRecentCopiedMSEGState;

    int oscilatorMenuIndex[n_scenes][n_oscs] = {0};

    bool hasIdleRun = false;
    VSTGUI::CPoint resizeToOnIdle = VSTGUI::CPoint(-1, -1);

  private:
    SGEDropAdapter *dropAdapter = nullptr;
    friend class SGEDropAdapter;

  public:
    bool canDropTarget(const std::string &fname); // these come as const char* from vstgui
    bool onDrop(const std::string &fname);

  private:
    VSTGUI::CRect positionForModulationGrid(modsources entry);

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

    void showMidiLearnOverlay(const VSTGUI::CRect &r);
    void hideMidiLearnOverlay();

  private:
    std::function<void(SurgeGUIEditor *, bool resizeWindow)> zoom_callback;
    bool zoomInvalid = false;
    int minimumZoom = 100;

    int selectedFX[8];
    std::string fxPresetName[8];

  public:
    std::shared_ptr<SurgeBitmaps> bitmapStore = nullptr;

  private:
    bool modsource_is_alternate[n_modsources];

  public:
    void toggleAlternateFor(VSTGUI::CControl *c);

  private:
    std::array<std::unique_ptr<Surge::Widgets::VuMeter>, 16> vu;
    std::unique_ptr<Surge::Widgets::PatchSelector> patchSelector;
    std::unique_ptr<Surge::Widgets::ParameterInfowindow> infowindow;

    std::unique_ptr<Surge::Widgets::EffectChooser> effectChooser;

    VSTGUI::CControlValueInterface *statusMPE = nullptr, *statusTune = nullptr,
                                   *statusZoom = nullptr;
    std::unique_ptr<Surge::Widgets::AboutScreen> aboutScreen;

    std::unique_ptr<juce::Drawable> midiLearnOverlay;

    VSTGUI::CTextEdit *patchName = nullptr;
    VSTGUI::CTextEdit *patchCategory = nullptr;
    VSTGUI::CTextEdit *patchCreator = nullptr;
    VSTGUI::CTextEdit *patchComment = nullptr;
    VSTGUI::CCheckBox *patchTuning = nullptr;
    VSTGUI::CTextLabel *patchTuningLabel = nullptr;
#if BUILD_IS_DEBUG
    VSTGUI::CTextLabel *debugLabel = nullptr;
#endif

    VSTGUI::CViewContainer *typeinDialog = nullptr;
    VSTGUI::CTextEdit *typeinValue = nullptr;
    VSTGUI::CTextLabel *typeinLabel = nullptr;
    VSTGUI::CTextLabel *typeinPriorValueLabel = nullptr;
    VSTGUI::CControl *typeinEditControl = nullptr;
    VSTGUI::CControlValueInterface *msegEditSwitch = nullptr;
    enum TypeInMode
    {
        Inactive,
        Param,
        Control
    } typeinMode = Inactive;
    std::vector<VSTGUI::CView *> removeFromFrame;
    int typeinResetCounter = -1;
    std::string typeinResetLabel = "";

    // Data structures for a list of overlays and associated data witht hem
    std::vector<std::pair<OverlayTags, VSTGUI::CViewContainer *>> editorOverlay;
    std::unordered_map<VSTGUI::CViewContainer *, VSTGUI::CView *>
        editorOverlayContentsWeakReference;
    std::unordered_map<VSTGUI::CViewContainer *, std::function<void()>> editorOverlayOnClose;

    VSTGUI::CViewContainer *minieditOverlay = nullptr;
    VSTGUI::CTextEdit *minieditTypein = nullptr;
    std::function<void(const char *)> minieditOverlayDone = [](const char *) {};

  public:
    std::string editorOverlayTagAtClose; // FIXME what is this?
    void promptForMiniEdit(const std::string &value, const std::string &prompt,
                           const std::string &title, const VSTGUI::CPoint &where,
                           std::function<void(const std::string &)> onOK);

    /*
     * This is the JUCE component management
     */
    std::array<std::unique_ptr<Surge::Widgets::EffectLabel>, 15> effectLabels;

    std::unordered_map<Surge::GUI::Skin::Control::sessionid_t, std::unique_ptr<juce::Component>>
        juceSkinComponents;
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

  private:
    std::unique_ptr<Surge::Widgets::VerticalLabel> lfoNameLabel;
    VSTGUI::CTextLabel *fxPresetLabel = nullptr;

  public:
    std::string modulatorName(int ms, bool forButton);

  private:
    Parameter *typeinEditTarget = nullptr;
    int typeinModSource = -1;

    VSTGUI::CControl *polydisp = nullptr;
    VSTGUI::CControl *oscdisplay = nullptr;
    VSTGUI::CControl *splitpointControl = nullptr;

    static const int n_paramslots = 1024;
    Surge::Widgets::ModulatableControlInterface *param[n_paramslots] = {};
    VSTGUI::CControlValueInterface *nonmod_param[n_paramslots] = {};
    CModulationSourceButton *gui_modsrc[n_modsources] = {};
    VSTGUI::CControl *metaparam[n_customcontrollers] = {};
    VSTGUI::CControl *lfodisplay = nullptr;
    Surge::Widgets::Switch *filtersubtype[2] = {};
    VSTGUI::CControl *fxmenu = nullptr;
    int clear_infoview_countdown = 0;

  public:
    int clear_infoview_peridle = -1;
    bool useDevMenu = false;

  private:
    float blinktimer = 0;
    bool blinkstate = false;
    SurgeSynthEditor *juceEditor{nullptr};
    int firstIdleCountdown = 0;

    /*
    ** Utility Function
    */
    std::shared_ptr<VSTGUI::CCommandMenuItem>
    addCallbackMenu(VSTGUI::COptionMenu *toThis, std::string label, std::function<void()> op);

    /*
     * Why have this? To avoid rewrites when porting from COptionMenu signature
     */
    void addCallbackMenu(juce::PopupMenu &menu, const std::string &label, std::function<void()> op)
    {
        menu.addItem(label, op);
    }

    juce::PopupMenu
    makeSmoothMenu(VSTGUI::CRect &menuRect, const Surge::Storage::DefaultKey &key, int defaultValue,
                   std::function<void(ControllerModulationSource::SmoothingMode)> setSmooth);

    juce::PopupMenu makeMpeMenu(VSTGUI::CRect &rect, bool showhelp);
    juce::PopupMenu makeTuningMenu(VSTGUI::CRect &rect, bool showhelp);
    juce::PopupMenu makeZoomMenu(VSTGUI::CRect &rect, bool showhelp);
    juce::PopupMenu makeSkinMenu(VSTGUI::CRect &rect);
    juce::PopupMenu makeUserSettingsMenu(VSTGUI::CRect &rect);
    juce::PopupMenu makeDataMenu(VSTGUI::CRect &rect);
    juce::PopupMenu makeMidiMenu(VSTGUI::CRect &rect);
    juce::PopupMenu makeDevMenu(VSTGUI::CRect &rect);
    juce::PopupMenu makeLfoMenu(VSTGUI::CRect &rect);
    VSTGUI::COptionMenu *makeMonoModeOptionsMenu(VSTGUI::CRect &rect, bool updateDefaults);

  public:
    bool getShowVirtualKeyboard();
    void setShowVirtualKeyboard(bool b);

  private:
    bool scannedForMidiPresets = false;

    void resetSmoothing(ControllerModulationSource::SmoothingMode t);
    void resetPitchSmoothing(ControllerModulationSource::SmoothingMode t);

  public:
    std::string helpURLFor(Parameter *p); // this requires internal state so doesn't have statics
    std::string helpURLForSpecial(std::string special); // these can be either this way or static

    static std::string helpURLForSpecial(SurgeStorage *, std::string special);
    static std::string fullyResolvedHelpURL(std::string helpurl);

  private:
    void promptForUserValueEntry(Parameter *p, VSTGUI::BaseViewFunctions *c,
                                 int modulationSource = -1);

    /*
    ** Skin support
    */
    Surge::GUI::Skin::ptr_t currentSkin;
    void setupSkinFromEntry(const Surge::GUI::SkinDB::Entry &entry);
    void reloadFromSkin();
    VSTGUI::CControlValueInterface *
    layoutComponentForSkin(std::shared_ptr<Surge::GUI::Skin::Control> skinCtrl, long tag,
                           int paramIndex = -1, Parameter *p = nullptr, int style = 0);

    /*
    ** General MIDI CC names
    */
    const char midicc_names[128][42] = {"Bank Select MSB",
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
                                        "Control 122 Local Control On/Of",
                                        "Control 123 All Notes Off",
                                        "Control 124 Omni Mode Off",
                                        "Control 125 Omni Mode On",
                                        "Control 126 Mono Mode Off",
                                        "Control 127 Mono Mode On"};
};
