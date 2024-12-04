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
#ifndef SURGE_SRC_COMMON_DSP_WAVETABLE_H
#define SURGE_SRC_COMMON_DSP_WAVETABLE_H
#include <string>
#include <StringOps.h>
const int max_wtable_size = 4096;
const int max_subtables = 512;
const int max_mipmap_levels = 16;

#pragma pack(push, 1)
struct wt_header
{
    // This struct can only contain scalar data that can be memcpy'd. It's read directly from data
    // on the disk.
    char tag[4];
    unsigned int n_samples;
    unsigned short n_tables;
    unsigned short flags;
};
#pragma pack(pop)

class Wavetable
{
  public:
    Wavetable();
    ~Wavetable();
    void Copy(Wavetable *wt);
    bool BuildWT(void *wdata, wt_header &wh, bool AppendSilence);
    void MipMapWT();

    void allocPointers(size_t newSize);

  public:
    bool everBuilt = false;
    int size;
    unsigned int n_tables;
    int size_po2;
    int flags;
    float dt;
    float *TableF32WeakPointers[max_mipmap_levels][max_subtables];
    short *TableI16WeakPointers[max_mipmap_levels][max_subtables];

    size_t dataSizes;
    float *TableF32Data;
    short *TableI16Data;

    int current_id, queue_id;
    bool refresh_display;
    bool is_dnd_imported;
    std::string queue_filename;
    std::string current_filename;
    int frame_size_if_absent{-1};
};

enum wtflags
{
    wtf_is_sample = 1,
    wtf_loop_sample = 2,
    wtf_int16 = 4,           // If this is set we have int16 in range 0-2^15
    wtf_int16_is_16 = 8,     // and in this case, range 0-2^16 if with above
    wtf_has_metadata = 0x10, // null term xml at end of file
};

#endif // SURGE_SRC_COMMON_DSP_WAVETABLE_H
