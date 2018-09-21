#pragma once
#include <string>
const int max_wtable_size = 1024;
const int max_subtables = 512;
const int max_mipmap_levels = 16;
const int max_wtable_samples = 268000; // delay pops 4 uses the most

#pragma pack(push, 1)
struct wt_header
{
   unsigned int tag;
   unsigned int n_samples;
   unsigned short n_tables, flags;
};
#pragma pack(pop)

class wavetable
{
public:
   wavetable();
   void Copy(wavetable* wt);
   bool BuildWT(void* wdata, wt_header& wh, bool AppendSilence);
   void MipMapWT();

public:
   int size;
   unsigned int n_tables;
   int size_po2;
   int flags;
   float dt;
   float* TableF32WeakPointers[max_mipmap_levels][max_subtables];
   short* TableI16WeakPointers[max_mipmap_levels][max_subtables];
   float TableF32Data[max_wtable_samples];
   short TableI16Data[max_wtable_samples];
   int current_id, queue_id;
   bool refresh_display;
   char queue_filename[256];
};

enum wtflags
{
   wtf_is_sample = 1,
   wtf_loop_sample = 2,
   wtf_int16 = 4,
};
