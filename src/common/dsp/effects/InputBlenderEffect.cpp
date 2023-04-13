
#include "InputBlenderEffect.h"
InputBlenderEffect::~InputBlenderEffect() {}
void InputBlenderEffect::init_ctrltypes() {
    Effect::init_ctrltypes();

    fxdata->p[ibp_channel].set_name("Channel");
    fxdata->p[ibp_channel].set_type(ct_input_blender_effect_channel);

    fxdata->p[ibp_audio_level].set_name("Audio Level");
    fxdata->p[ibp_audio_level].set_type(ct_input_blender_effect_audio_level);

    fxdata->p[ibp_upstream_level].set_name("Upstream Level");
    fxdata->p[ibp_upstream_level].set_type(ct_input_blender_effect_upstream_level);


}
