#pragma once
#include <string>
#include <vector>
#include <iostream>

namespace Surge
{
namespace Storage
{
struct Tone
{
   typedef enum Type
   {
      kToneCents,
      kToneRatio
   } Type;

   Type type;
   float cents;
   int ratio_d, ratio_n;
   std::string stringRep;
   float floatValue;

   Tone() : type(kToneRatio), cents(0), ratio_d(1), ratio_n(1), stringRep("1/1"), floatValue(1.0)
   {
   }
};

struct Scale
{
   std::string name;
   std::string description;
   int count;
   std::vector<Tone> tones;

   Scale() : name("empty scale"), description(""), count(0)
   {
   }
};

std::ostream& operator<<(std::ostream& os, const Tone& sc);
std::ostream& operator<<(std::ostream& os, const Scale& sc);

Scale readSCLFile(std::string fname);
} // namespace Storage
} // namespace Surge
