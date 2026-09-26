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
#include <vector>
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

    // Smallest frame size the runtime re-slicer will produce. BuildWT copes with anything
    // down to 1, but below this a "frame" stops being a waveform in any useful sense, and
    // the auto-shrink in Reslice would happily grind all the way to the bottom.
    static constexpr int min_reslice_size = 16;

    // Recover the source samples this wavetable was built from, by concatenating its top
    // mipmap level. Sample-mode tables carry trailing silent frames - either the three
    // BuildWT appends, or however many survived a patch round-trip - so those are trimmed
    // back off, which keeps repeated re-slices from accumulating padding.
    std::vector<float> FlattenSource() const;

    // Frames of real content, i.e. n_tables minus any trailing silent padding. For a
    // wavetable this is just n_tables; for a sample it is the count worth showing the user,
    // since the padding is an implementation detail of BuildWT.
    int SourceFrameCount() const;

    // Rebuild in place at a new frame size and/or frame count. newSize <= 0 keeps the
    // current frame size; newFrames <= 0 derives the largest frame count the samples
    // support. If newFrames does not fit at the requested frame size the size is halved
    // until it does (down to min_reslice_size), and any samples past the last whole frame
    // are truncated. Returns false and leaves the wavetable untouched if the request
    // cannot be satisfied.
    bool Reslice(int newSize, int newFrames, int newFlags);

  public:
    bool everBuilt = false;
    int size;
    // Frames actually populated from the build data, excluding any silence BuildWT
    // appended. FlattenSource needs this because n_tables counts the padding.
    int data_n_tables{0};
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
    bool force_refresh_display;
    bool refresh_script_editor;
    std::string queue_filename;
    std::string current_filename;
    int frame_size_if_absent{-1};

    // Runtime re-slice request. Set from the UI thread, consumed on the audio thread in
    // SurgeStorage::perform_queued_wtloads so the rebuild lands on a block boundary, the
    // same way a queued wavetable load does.
    bool queue_reslice{false};
    int reslice_size{-1};
    int reslice_frames{-1};
    int reslice_flags{0};
};

enum wtflags
{
    wtf_is_sample = 1,
    wtf_loop_sample = 2,
    wtf_int16 = 4,            // If this is set we have int16 in range 0-2^15
    wtf_int16_is_16 = 8,      // and in this case, range 0-2^16 if with above
    wtf_has_metadata = 0x10,  // null term xml at end of file
    wtf_user_modified = 0x20, // re-sliced at runtime, so it no longer matches its source
    // When set, the unison voice count is read as a sample play count instead, and the
    // oscillator collapses to a single voice. This is how samples have always behaved, so
    // the bit is set on patches predating it; a fresh load leaves it clear and gets real
    // unison, with wtf_loop_sample deciding whether the sample repeats forever.
    wtf_unison_is_loop_count = 0x40,
};

#endif // SURGE_SRC_COMMON_DSP_WAVETABLE_H
