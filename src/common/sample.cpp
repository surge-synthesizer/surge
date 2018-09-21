//-------------------------------------------------------------------------------------------------------
//
//	Shortcircuit
//
//	Copyright 2004 Claes Johanson
//
//-------------------------------------------------------------------------------------------------------
#if WINDOWS
#include "sample.h"
#include <assert.h>

sample::sample()
{
   sample_loaded = false;
   sample_data = 0;
   refcount = 1;
}

sample::~sample()
{
   if (sample_loaded)
   {
      delete sample_data;
   }
}

bool sample::load(const char* file_name)
{
   assert(file_name);
   char filename[256];
   strcpy(filename, file_name);

   // find the extension
   char* extension = strrchr(filename, '.');
   if (!extension)
      return false;
   extension += 1;

   // determine if there is a comma in the name (for sf2 & gig adressing)
   char samplename[32];
   samplename[0] = 0;
   char* comma = strrchr(filename, ',');
   if (comma && (comma > extension))
   {
      strcpy(samplename, comma + 1);
      *comma = 0;
   }

   if (!stricmp(extension, "wav"))
   {
      bool result = this->load_riff_wave_mk2(filename);
      return result;
   }
   return false;
}

#endif