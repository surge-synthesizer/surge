#include "SurgeLv2Ui.h"
#include "SurgeLv2Util.h"
#include "SurgeLv2Vstgui.h"
#include "SurgeLv2Wrapper.h"
#include "SurgeGUIEditor.h"
#include <cassert>

#include "CScalableBitmap.h"

SurgeLv2Ui::SurgeLv2Ui(SurgeLv2Wrapper* instance,
                       void* parentWindow,
                       const LV2_URID_Map* uridMapper,
                       const LV2UI_Resize* uiResizer,
                       LV2UI_Write_Function writeFn,
                       LV2UI_Controller controller,
                       float uiScaleFactor)
    : _editor(new SurgeGUIEditor(instance, instance->synthesizer(), this)), _instance(instance),
      _writeFn(writeFn), _controller(controller),
      _uiScaleFactor(uiScaleFactor), _uiInitialized(false)
{
   instance->setEditor(this);

   if (uiResizer)
   {
      _editor->setZoomFactor(uiScaleFactor * 100);
      _editor->setZoomCallback(
         [this, uiResizer](SurgeGUIEditor* editor) { handleZoom(editor, uiResizer); });
   }

   _editor->open(parentWindow);

   if (uiResizer)
   {
      VSTGUI::ERect* rect = nullptr;
      _editor->getRect(&rect);
      uiResizer->ui_resize(uiResizer->handle,
                           (rect->right - rect->left) * uiScaleFactor,
                           (rect->bottom - rect->top) * uiScaleFactor);
   }

   _uiInitialized = true;
}

SurgeLv2Ui::~SurgeLv2Ui()
{
   _instance->setEditor(nullptr);
   _editor->close();
}

void SurgeLv2Ui::setParameterAutomated(int externalparam, float value)
{
   _writeFn(_controller, externalparam, sizeof(float), 0, &value);
}

#if LINUX
void SurgeLv2Ui::associateIdleRunLoop(const VSTGUI::SharedPointer<Lv2IdleRunLoop>& runLoop)
{
   _runLoop = runLoop;
}
#endif

LV2UI_Descriptor SurgeLv2Ui::createDescriptor()
{
   LV2UI_Descriptor desc = {};
   desc.URI = SURGE_UI_URI;
   desc.instantiate = &instantiate;
   desc.cleanup = &cleanup;
   desc.port_event = &portEvent;
   desc.extension_data = &extensionData;
   return desc;
}

LV2UI_Handle SurgeLv2Ui::instantiate(const LV2UI_Descriptor* descriptor,
                                     const char* plugin_uri,
                                     const char* bundle_path,
                                     LV2UI_Write_Function write_function,
                                     LV2UI_Controller controller,
                                     LV2UI_Widget* widget,
                                     const LV2_Feature* const* features)
{
   SurgeLv2Wrapper* instance =
       (SurgeLv2Wrapper*)SurgeLv2::requireFeature(LV2_INSTANCE_ACCESS_URI, features);
   void* parentWindow = (void*)SurgeLv2::findFeature(LV2_UI__parent, features);
   auto* featureUridMap = (const LV2_URID_Map*)SurgeLv2::requireFeature(LV2_URID__map, features);
   auto* featureResize = (const LV2UI_Resize*)SurgeLv2::findFeature(LV2_UI__resize, features);
   auto* featureOptions = (const LV2_Options_Option*)SurgeLv2::findFeature(LV2_OPTIONS__options, features);

   auto uriScaleFactor = featureUridMap->map(featureUridMap->handle, LV2_UI__scaleFactor);
   float uiScaleFactor = 1.0f;
   for (int i=0; featureOptions[i].key != 0; ++i)
   {
        if (featureOptions[i].key == uriScaleFactor)
        {
            if (featureOptions[i].type == featureUridMap->map(featureUridMap->handle, LV2_ATOM__Float))
                uiScaleFactor = *(const float*)featureOptions[i].value;
            break;
        }
   }

   std::unique_ptr<SurgeLv2Ui> ui(new SurgeLv2Ui(instance, parentWindow, featureUridMap,
                                                 featureResize, write_function, controller, uiScaleFactor));
   return (LV2UI_Handle)ui.release();
}

void SurgeLv2Ui::cleanup(LV2UI_Handle ui)
{
   SurgeLv2Ui* self = (SurgeLv2Ui*)ui;
   delete self;
}

void SurgeLv2Ui::portEvent(
    LV2UI_Handle ui, uint32_t port_index, uint32_t buffer_size, uint32_t format, const void* buffer)
{
   SurgeLv2Ui* self = (SurgeLv2Ui*)ui;
   SurgeSynthesizer* s = self->_instance->synthesizer();

   if (format == 0 && port_index < n_total_params)
   {
      assert(buffer_size == sizeof(float));

      unsigned index = s->remapExternalApiToInternalId(port_index);
      float value = *(const float*)buffer;
      bool external = true;

      s->setParameter01(index, value, external);
   }
}

const void* SurgeLv2Ui::extensionData(const char* uri)
{
   if (!strcmp(uri, LV2_UI__idleInterface))
   {
      static const LV2UI_Idle_Interface idle = {&uiIdle};
      return &idle;
   }
   // if (!strcmp(uri, LV2_UI__resize)) {
   //     static const LV2UI_Resize resize = { nullptr, &uiResize };
   //     return &resize;
   // }
   return nullptr;
}

int SurgeLv2Ui::uiIdle(LV2UI_Handle ui)
{
   SurgeLv2Ui* self = (SurgeLv2Ui*)ui;
   SurgeLv2Wrapper* instance = self->_instance;

   // HACK to get parameters written to host
   // if a patch was loaded from storage, the plugin atomically sets a bit to
   // indicate it. In this case, write the control value again using UI's write
   // interface, in order to update the host side.
   if (instance->_editorMustReloadPatch.exchange(false)) {
      SurgeSynthesizer *s = instance->synthesizer();
      for (unsigned int i = 0; i < n_total_params; i++)
      {
         unsigned index = s->remapExternalApiToInternalId(i);
         float value = s->getParameter01(index);
         self->setParameterAutomated(i, value);
      }
   }

#if LINUX
   self->_runLoop->execIdle();
#endif

   // TODO maybe handle the case of closed window here
   return 0;
}

void SurgeLv2Ui::handleZoom(SurgeGUIEditor *e, const LV2UI_Resize* resizer)
{
   assert(e == _editor.get());

    float fzf = e->getZoomFactor() / 100.0;
    int newW = WINDOW_SIZE_X * fzf;
    int newH = WINDOW_SIZE_Y * fzf;

    // identical implementation to VST2, except for here
    if (_uiInitialized)
        resizer->ui_resize(resizer->handle, newW, newH);

    VSTGUI::CFrame *frame = e->getFrame();
    if(frame)
    {
        frame->setZoom(fzf);
        /*
        ** cframe::setZoom uses prior size and integer math which means repeated resets drift
        ** look at the "oroginWidth" math around in CFrame::setZoom. So rather than let those
        ** drifts accumulate, just reset the size here since we know it
        */
        frame->setSize(newW, newH);

        /*
        ** VST2 has an error which is that the background bitmap doesn't get the frame transform
        ** applied. Simply look at cviewcontainer::drawBackgroundRect. So we have to force the background
        ** scale up using a backdoor API.
        */

        VSTGUI::CBitmap *bg = frame->getBackground();
        if(bg != NULL)
        {
            CScalableBitmap *sbm = dynamic_cast<CScalableBitmap *>(bg); // dynamic casts are gross but better safe
            if (sbm)
            {
               sbm->setExtraScaleFactor(e->getZoomFactor());
            }
        }

        frame->setDirty(true);
        frame->invalid();
    }
}

///
#if LINUX
#include "vstgui/lib/platform/platform_x11.h"
#include "vstgui/lib/platform/linux/x11platform.h"

namespace SurgeLv2
{
VSTGUI::SharedPointer<VSTGUI::X11::IRunLoop> createRunLoop(void* ui)
{
   SurgeLv2Ui* self = (SurgeLv2Ui*)ui;
   VSTGUI::SharedPointer<Lv2IdleRunLoop> runLoop(new Lv2IdleRunLoop);
   self->associateIdleRunLoop(runLoop);
   return runLoop;
}
} // namespace SurgeLv2
#endif // LINUX

///
LV2_SYMBOL_EXPORT
const LV2UI_Descriptor* lv2ui_descriptor(uint32_t index)
{
#if LINUX
   VSTGUI::initializeSoHandle();
#endif

   if (index == 0)
   {
      static LV2UI_Descriptor desc = SurgeLv2Ui::createDescriptor();
      return &desc;
   }
   return nullptr;
}
