#pragma once
#include <string>
#include <vector>
#include <iostream>

class SurgeStorage;

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

   std::string toHtml(SurgeStorage *storage);

   static Scale evenTemprament12NoteScale();
};

struct KeyboardMapping
{
   bool isValid;
   bool isStandardMapping;
   int count;
   int firstMidi, lastMidi;
   int middleNote;
   int tuningConstantNote;
   float tuningFrequency;
   int octaveDegrees;
   std::vector<int> keys; // rather than an 'x' we use a '-1' for skipped keys

   std::string rawText;
   std::string name;
   
   KeyboardMapping() : isValid(true),
                       isStandardMapping(true),
                       count(12),
                       firstMidi(0),
                       lastMidi(127),
                       middleNote(60),
                       tuningConstantNote(60),
                       tuningFrequency(8.175798915 * 32),
                       octaveDegrees(12),
                       rawText( "" ),
                       name( "" )
      {
         for( int i=0; i<12; ++i )
            keys.push_back(i);
      }

   // TODO
   // std::string toHtml(); 
};
   
std::ostream& operator<<(std::ostream& os, const Tone& sc);
std::ostream& operator<<(std::ostream& os, const Scale& sc);
//TODO
//std::ostream& operator<<(std::ostream& os, const KeyboardMapping& kbm);
   
Scale readSCLFile(std::string fname);
Scale parseSCLData(const std::string &sclContents);
KeyboardMapping readKBMFile(std::string fname);
KeyboardMapping parseKBMData(const std::string &kbmContents);
} // namespace Storage
} // namespace Surge
