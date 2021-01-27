// This file is part of VSTGUI. It is subject to the license terms
// in the LICENSE file found in the top-level directory of this
// distribution and at http://github.com/steinbergmedia/vstgui/LICENSE

#if TARGET_VST2

#ifndef __linux_aeffguieditor__
#define __linux_aeffguieditor__

#ifndef __aeffeditor__
#include "public.sdk/source/vst2.x/aeffeditor.h"
#endif

#ifndef __audioeffectx__
#include "public.sdk/source/vst2.x/audioeffectx.h"
#endif

#include "vstgui/vstgui.h"

namespace VSTGUI
{

//-----------------------------------------------------------------------------
// LinuxAEffGUIEditor Declaration
//-----------------------------------------------------------------------------
class LinuxAEffGUIEditor : public AEffEditor, public VSTGUIEditorInterface
{
  public:
    LinuxAEffGUIEditor(void *pEffect);

    ~LinuxAEffGUIEditor() override;

    virtual void setParameter(VstInt32 index, float value) {}
    bool getRect(ERect **ppRect) override;
    bool open(void *ptr) override;
    bool idle2();

#if VST_2_1_EXTENSIONS
    bool onKeyDown(VstKeyCode &keyCode) override;
    bool onKeyUp(VstKeyCode &keyCode) override;
#endif

    // get the effect attached to this editor
    AudioEffect *getEffect() override { return effect; }

    // get version of this VSTGUI
    int32_t getVstGuiVersion() { return (VSTGUI_VERSION_MAJOR << 16) + VSTGUI_VERSION_MINOR; }

    // set/get the knob mode
    bool setKnobMode(int32_t val) override;
    int32_t getKnobMode() const override { return knobMode; }

    bool beforeSizeChange(const CRect &newSize, const CRect &oldSize) override;

#if VST_2_1_EXTENSIONS
    void beginEdit(int32_t index) override { ((AudioEffectX *)effect)->beginEdit(index); }
    void endEdit(int32_t index) override { ((AudioEffectX *)effect)->endEdit(index); }
#endif

    //---------------------------------------
  protected:
    bool inIdle;
    ERect rect;

  private:
    static int32_t knobMode;
};

} // namespace VSTGUI

#endif // include guard

#endif // TARGET VST2
