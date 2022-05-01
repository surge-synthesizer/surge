#pragma once
#include <string>
const int max_wtable_size = 4096;
const int max_subtables = 512;
const int max_mipmap_levels = 16;
// I don't know why your max wtable samples would be less than your max tables * your max sample
// size. So lets fix that! This size is consistent with the check in WaveTable.cpp //
// CheckRequiredWTSize with ts and tc at 1024 and 512
const int max_wtable_samples = 2097152;
// const int max_wtable_samples =  268000; // delay pops 4 uses the most

#pragma pack(push, 1)
struct wt_header
{
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
    char queue_filename[256];
    char current_filename[256];
};

enum wtflags
{
    wtf_is_sample = 1,
    wtf_loop_sample = 2,
    wtf_int16 = 4,       // If this is set we have int16 in range 0-2^15
    wtf_int16_is_16 = 8, // and in this case, range 0-2^16 if with above
};
