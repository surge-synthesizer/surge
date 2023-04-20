
#include "InputBlenderEffect.h"
InputBlenderEffect::InputBlenderEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd)
    : Effect(storage, fxdata, pd) {}
InputBlenderEffect::~InputBlenderEffect() {}
void InputBlenderEffect::init_ctrltypes()
{
    Effect::init_ctrltypes();

    ctrltypes ctrltype = ct_none;
    switch (fxdata->fxslot)
    {
    case fxslot_ains1:
    case fxslot_ains2:
    case fxslot_ains3:
    case fxslot_ains4:
        ctrltype = ct_input_channel_withB;
        break;
    case fxslot_bins1:
    case fxslot_bins2:
    case fxslot_bins3:
    case fxslot_bins4:
        ctrltype = ct_input_channel_withA;
        break;
    default:
        ctrltype = ct_input_channel;
    }
    fxdata->p[ibp_channel].set_name("Channel");
    fxdata->p[ibp_channel].set_type(ctrltype);
    fxdata->p[ibp_channel].posy_offset = 1;

    fxdata->p[ibp_input_level].set_name("Input Level");
    fxdata->p[ibp_input_level].set_type(ct_decibel_attenuation);
    fxdata->p[ibp_input_level].posy_offset = 1;

    fxdata->p[ibp_input_width].set_name("Input Width");
    fxdata->p[ibp_input_width].set_type(ct_percent_bipolar_stereo);
    fxdata->p[ibp_input_width].posy_offset = 1;

    fxdata->p[ibp_upstream_level].set_name("Upstream Level");
    fxdata->p[ibp_upstream_level].set_type(ct_decibel_attenuation);
    fxdata->p[ibp_upstream_level].posy_offset = 4;

    fxdata->p[ibp_upstream_width].set_name("Upstream Width");
    fxdata->p[ibp_upstream_width].set_type(ct_percent_bipolar_stereo);
    fxdata->p[ibp_upstream_width].posy_offset = 4;

    fxdata->p[ibp_output_mix].set_name("Output Mix");
    fxdata->p[ibp_output_mix].set_type(ct_percent_bipolar);
    fxdata->p[ibp_output_mix].posy_offset = 7;

    fxdata->p[ibp_output_width].set_name("Output Width");
    fxdata->p[ibp_output_width].set_type(ct_percent_bipolar_stereo);
    fxdata->p[ibp_output_width].posy_offset = 7;

}

void InputBlenderEffect::init_default_values()
{
    fxdata->p[ibp_channel].val.i = 0;
    fxdata->p[ibp_input_level].val.f = 0.0;
    fxdata->p[ibp_upstream_level].val.f = 0.0;
}
const char *InputBlenderEffect::group_label(int id) {
    switch (id)
    {
    case 0:
        return "Audio Input";
    case 1:
        return "Upstream";
    case 3:
        return "Output";
    }
    return 0;
}

int InputBlenderEffect::group_label_ypos(int id) {
    switch (id)
    {
    case 0:
            return 1;
    case 1:
            return 10;
    case 3:
        return 17;
    }
    return 0;
}
void InputBlenderEffect::process(float *dataL, float *dataR)
{
    Effect::process(dataL, dataR);
}

