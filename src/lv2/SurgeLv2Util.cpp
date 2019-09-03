#include "SurgeLv2Util.h"
#include <stdexcept>
#include <cstdio>

namespace SurgeLv2
{

const LV2_Atom_Event* popNextEvent(const LV2_Atom_Sequence* sequence,
                                   const LV2_Atom_Event** iterator)
{
   const LV2_Atom_Event* next = nullptr;
   if (!lv2_atom_sequence_is_end(&sequence->body, sequence->atom.size, *iterator))
   {
      next = *iterator;
      *iterator = lv2_atom_sequence_next(next);
   }
   return next;
}

const LV2_Atom_Event* popNextEventAtFrame(const LV2_Atom_Sequence* sequence,
                                          const LV2_Atom_Event** iterator,
                                          uint32_t frameIndex)
{
   const LV2_Atom_Event* next = nullptr;
   if (!lv2_atom_sequence_is_end(&sequence->body, sequence->atom.size, *iterator) &&
       frameIndex <= (uint32_t)(*iterator)->time.frames)
   {
      next = *iterator;
      *iterator = lv2_atom_sequence_next(next);
   }
   return next;
};

const void* findFeature(const char* uri, const LV2_Feature* const* features)
{
   for (const LV2_Feature* const* fp = features; *fp; ++fp)
      if (!strcmp((*fp)->URI, uri))
         return (*fp)->data;
   return nullptr;
};

const void* requireFeature(const char* uri, const LV2_Feature* const* features)
{
   const void* feature = findFeature(uri, features);
   if (!feature)
   {
      fprintf(stderr, "Surge LV2: cannot find required feature <%s>\n", uri);
      throw std::runtime_error("LV2 required feature missing");
   }
   return feature;
}

} // namespace SurgeLv2
