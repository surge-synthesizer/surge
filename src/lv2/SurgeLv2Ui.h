#pragma once
#include "AllLv2.h"
#include "vstgui/lib/vstguibase.h"
#include <memory>

class SurgeLv2Wrapper;
class SurgeGUIEditor;
class Lv2IdleRunLoop;

namespace VSTGUI
{
class CBitmap;
}

class SurgeLv2Ui
{
  public:
    static LV2UI_Descriptor createDescriptor();

    explicit SurgeLv2Ui(SurgeLv2Wrapper *instance, void *parentWindow,
                        const LV2_URID_Map *uridMapper, const LV2UI_Resize *uiResizer,
                        LV2UI_Write_Function writeFn, LV2UI_Controller controller,
                        float uiScaleFactor);
    ~SurgeLv2Ui();

    void setParameterAutomated(int externalparam, float value);

#if LINUX
    void associateIdleRunLoop(const VSTGUI::SharedPointer<Lv2IdleRunLoop> &runLoop);
#endif

  private:
    static LV2UI_Handle instantiate(const LV2UI_Descriptor *descriptor, const char *plugin_uri,
                                    const char *bundle_path, LV2UI_Write_Function write_function,
                                    LV2UI_Controller controller, LV2UI_Widget *widget,
                                    const LV2_Feature *const *features);
    static void cleanup(LV2UI_Handle ui);
    static void portEvent(LV2UI_Handle ui, uint32_t port_index, uint32_t buffer_size,
                          uint32_t format, const void *buffer);
    static const void *extensionData(const char *uri);
    static int uiIdle(LV2UI_Handle ui);

    // callback from the editor
    void handleZoom(SurgeGUIEditor *e, const LV2UI_Resize *resizer, bool resizeWindow);
    void setExtraScaleFactor(VSTGUI::CBitmap *bg, float zf);

  private:
    std::unique_ptr<SurgeGUIEditor> _editor;
    SurgeLv2Wrapper *_instance;
#if LINUX
    VSTGUI::SharedPointer<Lv2IdleRunLoop> _runLoop;
#endif
    LV2UI_Write_Function _writeFn;
    LV2UI_Controller _controller;
    float _uiScaleFactor;
    bool _uiInitialized;
};
