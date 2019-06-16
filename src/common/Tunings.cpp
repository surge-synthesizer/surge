
#include "Tunings.h"

#include <iostream>
#include <fstream>
#include <cstdlib>

/*
 From: http://huygens-fokker.org/scala/scl_format.html
 The files are human readable ASCII or 8-bit character text-files.
 The file type is .scl .
 There is one scale per file.
 Lines beginning with an exclamation mark are regarded as comments and are to be ignored.
 The first (non comment) line contains a short description of the scale, but long lines are possible
 and should not give a read error. The description is only one line. If there is no description,
 there should be an empty line. The second line contains the number of notes. This number indicates
 the number of lines with pitch values that follow. In principle there is no upper limit to this,
 but it is allowed to reject files exceeding a certain size. The lower limit is 0, which is possible
 since degree 0 of 1/1 is implicit. Spaces before or after the number are allowed.

 After that come the pitch values, each on a separate line, either as a ratio or as a value in
 cents. If the value contains a period, it is a cents value, otherwise a ratio. Ratios are written
 with a slash, and only one. Integer values with no period or slash should be regarded as such, for
 example "2" should be taken as "2/1". Numerators and denominators should be supported to at least
 231-1 = 2147483647. Anything after a valid pitch value should be ignored. Space or horizontal tab
 characters are allowed and should be ignored. Negative ratios are meaningless and should give a
 read error. For a description of cents, go here. The first note of 1/1 or 0.0 cents is implicit and
 not in the files. Files for which Scala gives Error in file format are incorrectly formatted. They
 should give a read error and be rejected.
*/

Surge::Storage::Scale Surge::Storage::readSCLFile(std::string fname)
{
   std::ifstream inf;
   inf.open(fname);
   if (!inf.is_open())
   {
      return Scale();
   }

   std::string line;
   const int read_header = 0, read_count = 1, read_note = 2;
   int state = read_header;

   Scale res;
   res.name = fname;
   while (std::getline(inf, line))
   {
      if (line[0] == '!')
      {
         continue;
      }
      switch (state)
      {
      case read_header:
         res.description = line;
         state = read_count;
         break;
      case read_count:
         res.count = atoi(line.c_str());
         state = read_note;
         break;
      case read_note:
         Tone t;
         t.stringRep = line;
         if (line.find(".") != std::string::npos)
         {
            t.type = Tone::kToneCents;
            t.cents = atof(line.c_str());
            t.floatValue = t.cents / 1200.0 + 1.0;
         }
         else
         {
            t.type = Tone::kToneRatio;
            auto slashPos = line.find("/");
            if (slashPos == std::string::npos)
            {
               t.ratio_d = atoi(line.c_str());
               t.ratio_n = 1;
            }
            else
            {
               t.ratio_n = atoi(line.substr(0, slashPos).c_str());
               t.ratio_d = atoi(line.substr(slashPos + 1).c_str());
            }
            t.floatValue = 1.0 * t.ratio_n / t.ratio_d;
         }
         res.tones.push_back(t);

         break;
      }
   }

   return res;
}

std::ostream& Surge::Storage::operator<<(std::ostream& os, const Surge::Storage::Tone& t)
{
   os << (t.type == Tone::kToneCents ? "cents" : "ratio") << "  ";
   if (t.type == Tone::kToneCents)
      os << t.cents;
   else
      os << t.ratio_n << " / " << t.ratio_d;
   os << "  (" << t.floatValue << ")";
   return os;
}

std::ostream& Surge::Storage::operator<<(std::ostream& os, const Surge::Storage::Scale& sc)
{
   os << "Scale\n  " << sc.name << "\n  " << sc.description << "\n  --- " << sc.count
      << " tones ---\n";
   for (auto t : sc.tones)
      os << "    - " << t << "\n";
   return os;
}
