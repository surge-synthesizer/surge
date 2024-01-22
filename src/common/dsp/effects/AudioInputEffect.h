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
#ifndef SURGE_SRC_COMMON_DSP_EFFECTS_AUDIOINPUTEFFECT_H
#define SURGE_SRC_COMMON_DSP_EFFECTS_AUDIOINPUTEFFECT_H
#include "Effect.h"
class AudioInputEffect : public Effect
{
  public:
    enum effect_slot_type
    {
        a_insert_slot,
        b_insert_slot,
        send_slot,
        global_slot
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
    ~AudioInputEffect() = default;
    void init_ctrltypes() override;
    void init_default_values() override;
    void process(float *dataL, float *dataR) override;
    const char *group_label(int id) override;
    int group_label_ypos(int id) override;
    void init() override;

  private:
    lipol_ps_blocksz mix alignas(16), width alignas(16);

    struct SliderSmoothing
    {
        using lip = sst::basic_blocks::dsp::lipol_sse<BLOCK_SIZE, true>;
        lip leftG alignas(16), rightG alignas(16);
        lip leftP alignas(16), rightP alignas(16);
        lip omleftP alignas(16), omrightP alignas(16);
        lip level alignas(16);

        void resetFirstRun()
        {
            leftG.first_run = true;
            leftP.first_run = true;
            omleftP.first_run = true;
            rightG.first_run = true;
            rightP.first_run = true;
            omrightP.first_run = true;
            level.first_run = true;
        }
    } sliderSmooths[3];

    std::shared_ptr<float> sceneDataPtr[N_OUTPUTS]{nullptr, nullptr};
    effect_slot_type getSlotType(fxslot_positions p);

    template <int whichInstance>
    void applySlidersControls(float *buffer[], const float &channel, const float &pan,
                              const float &levelDb);
};

#endif // SURGE_SRC_COMMON_DSP_EFFECTS_AUDIOINPUTEFFECT_H
