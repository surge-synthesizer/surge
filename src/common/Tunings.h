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
   std::string rawText;
   int count;
   std::vector<Tone> tones;

   Scale() : name("empty scale"), description(""), rawText(""), count(0)
   {
   }

   bool isValid() const;

   std::string toHtml();
};

std::ostream& operator<<(std::ostream& os, const Tone& sc);
std::ostream& operator<<(std::ostream& os, const Scale& sc);

Scale readSCLFile(std::string fname);
Scale parseSCLData(const std::string &sclContents);
} // namespace Storage
} // namespace Surge
