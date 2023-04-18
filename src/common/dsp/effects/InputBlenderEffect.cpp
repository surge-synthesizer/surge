
#include "InputBlenderEffect.h"
InputBlenderEffect::InputBlenderEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd)
    : Effect(storage, fxdata, pd) {}
InputBlenderEffect::~InputBlenderEffect() {}
void InputBlenderEffect::init_ctrltypes()
{
    Effect::init_ctrltypes();

    fxdata->p[ibp_channel].set_name("Channel");
    fxdata->p[ibp_channel].set_type( ct_input_channel);

    fxdata->p[ibp_audio_level].set_name("Audio Level");
    fxdata->p[ibp_audio_level].set_type(ct_decibel_attenuation);

    fxdata->p[ibp_upstream_level].set_name("Upstream Level");
    fxdata->p[ibp_upstream_level].set_type(ct_decibel_attenuation);

}

void InputBlenderEffect::init_default_values()
{
    fxdata->p[ibp_channel].val.i = 0;
    fxdata->p[ibp_audio_level].val.f = 0.0;
    fxdata->p[ibp_upstream_level].val.f = 0.0;
}
void InputBlenderEffect::process(float *dataL, float *dataR)
{
    Effect::process(dataL, dataR);
}

