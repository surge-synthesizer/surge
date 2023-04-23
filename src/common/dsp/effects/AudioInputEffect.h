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
#include "Effect.h"

class AudioInputEffect : public Effect
{
  public:
    enum effect_slot_type{
        a_insert_slot, b_insert_slot, send_slot, global_slot
    };
    enum in_params
    {
        in_audio_input_channel = 0,
        in_audio_input_pan,
        in_audio_input_level,

        in_effect_input_channel,
        in_effect_input_pan,
        in_effect_input_level,

        in_scene_input_channel,
        in_scene_input_pan,
        in_scene_input_level,

        in_output_width,
        in_output_mix,

        in_num_params
    };
    AudioInputEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd);
    ~AudioInputEffect() override;
    void init_ctrltypes() override;
    void init_default_values() override;
    void process(float *dataL, float *dataR) override;
    const char *group_label(int id) override;
    int group_label_ypos(int id) override;
  private:
    effect_slot_type getSlotType(fxslot_positions p);
};

