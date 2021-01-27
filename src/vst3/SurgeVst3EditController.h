#pragma once

#include "public.sdk/source/vst/vsteditcontroller.h"

using namespace Steinberg;
using namespace Steinberg::Vst;

class SurgeEditorView;
#if !TARGET_AUDIOUNIT && !MAC

class SurgeVst3EditController : public EditControllerEx1, public IMidiMapping
{
  public:
    //------------------------------------------------------------------------
    // create function required for Plug-in factory,
    // it will be called to create new instances of this controller
    //------------------------------------------------------------------------
    static FUnknown *createInstance(void *context)
    {
        return 0; //(IEditController*)new SurgeController;
    }

    //---from IPluginBase--------
    tresult PLUGIN_API initialize(FUnknown *context);
    tresult PLUGIN_API terminate();

    //---from EditController-----
    tresult PLUGIN_API setComponentState(IBStream *state);
    IPlugView *PLUGIN_API createView(const char *name);
    tresult PLUGIN_API setState(IBStream *state);
    tresult PLUGIN_API getState(IBStream *state);
    tresult PLUGIN_API setParamNormalized(ParamID tag, ParamValue value);
    tresult PLUGIN_API getParamStringByValue(ParamID tag, ParamValue valueNormalized,
                                             String128 string);
    tresult PLUGIN_API getParamValueByString(ParamID tag, TChar *string,
                                             ParamValue &valueNormalized);
    void editorDestroyed(EditorView *editor) {} // nothing to do here
    void editorAttached(EditorView *editor);
    void editorRemoved(EditorView *editor);

    //---from ComponentBase-----
    tresult receiveText(const char *text);

    //---from IMidiMapping-----------------
    tresult PLUGIN_API getMidiControllerAssignment(int32 busIndex, int16 channel,
                                                   CtrlNumber midiControllerNumber, ParamID &tag);
    DELEGATE_REFCOUNT(EditController)

    tresult PLUGIN_API queryInterface(const char *iid, void **obj);

    //---Internal functions-------
    void addDependentView(SurgeEditorView *view);
    void removeDependentView(SurgeEditorView *view);

    void setDefaultMessageText(String128 text);
    TChar *getDefaultMessageText();
    //------------------------------------------------------------------------

  private:
    String128 defaultMessageText;
};
#endif
