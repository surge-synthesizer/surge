
#include "Tunings.h"
#include "SurgeStorage.h"

#include <iostream>
#include <fstream>
#include <cstdlib>
#include <math.h>
#include <sstream>

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

Surge::Storage::Scale scaleFromStream(std::istream &inf)
{
   std::string line;
   const int read_header = 0, read_count = 1, read_note = 2;
   int state = read_header;

   Surge::Storage::Scale res;
   std::ostringstream rawOSS;
   while (std::getline(inf, line))
   {
      rawOSS << line << "\n";
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
         Surge::Storage::Tone t;
         t.stringRep = line;
         if (line.find(".") != std::string::npos)
         {
            t.type = Surge::Storage::Tone::kToneCents;
            t.cents = atof(line.c_str());
         }
         else
         {
            t.type = Surge::Storage::Tone::kToneRatio;
            auto slashPos = line.find("/");
            if (slashPos == std::string::npos)
            {
               t.ratio_n = atoi(line.c_str());
               t.ratio_d = 1;
            }
            else
            {
               t.ratio_n = atoi(line.substr(0, slashPos).c_str());
               t.ratio_d = atoi(line.substr(slashPos + 1).c_str());
            }

            // 2^(cents/1200) = n/d
            // cents = 1200 * log(n/d) / log(2)
            
            t.cents = 1200 * log(1.0 * t.ratio_n/t.ratio_d) / log(2.0);
         }
         t.floatValue = t.cents / 1200.0 + 1.0;
         res.tones.push_back(t);

         break;
      }
   }

   res.rawText = rawOSS.str();
   return res;
}

Surge::Storage::Scale Surge::Storage::readSCLFile(std::string fname)
{
   std::ifstream inf;
   inf.open(fname);
   if (!inf.is_open())
   {
      return Scale();
   }

   auto res = scaleFromStream(inf);
   res.name = fname;
   return res;
}

Surge::Storage::Scale Surge::Storage::parseSCLData(const std::string &d)
{
    std::istringstream iss(d);
    auto res = scaleFromStream(iss);
    res.name = "Scale from Patch";
    return res;
}

Surge::Storage::Scale Surge::Storage::Scale::evenTemperament12NoteScale()
{
   auto data = R"SCL(! even.scl
!
12 note even temprament
 12
!
 100.0
 200.0
 300.0
 400.0
 500.0
 600.0
 700.0
 800.0
 900.0
 1000.0
 1100.0
 2/1
)SCL";
   return parseSCLData(data);
}

Surge::Storage::KeyboardMapping keyboardMappingFromStream(std::istream &inf)
{
   std::string line;
   const int read_header = 0, read_count = 1, read_note = 2;

   Surge::Storage::KeyboardMapping res;
   std::ostringstream rawOSS;
   res.isStandardMapping = false;
   res.keys.clear();

   enum parsePosition {
      map_size = 0,
      first_midi,
      last_midi,
      middle,
      reference,
      freq,
      degree,
      keys
   };
   parsePosition state = map_size;
   
   while (std::getline(inf, line))
   {
      rawOSS << line << "\n";
      if (line[0] == '!')
      {
         continue;
      }

      if( line == "x" ) line = "-1";
      
      int i = std::atoi(line.c_str());
      float v = std::atof(line.c_str());

      switch (state)
      {
      case map_size:
         res.count = i;
         break;
      case first_midi:
         res.firstMidi = i;
         break;
      case last_midi:
         res.lastMidi = i;
         break;
      case middle:
         res.middleNote = i;
         break;
      case reference:
         res.tuningConstantNote = i;
         break;
      case freq:
         res.tuningFrequency = v;
         break;
      case degree:
         res.octaveDegrees = i;
         break;
      case keys:
         res.keys.push_back(i);
         break;
      }
      if( state != keys ) state = (parsePosition)(state + 1);
   }

   res.rawText = rawOSS.str();
   return res;
}

Surge::Storage::KeyboardMapping Surge::Storage::readKBMFile(std::string fname)
{
   std::ifstream inf;
   inf.open(fname);
   if (!inf.is_open())
   {
      return KeyboardMapping();
   }

   auto res = keyboardMappingFromStream(inf);
   res.name = fname;
   return res;
}

Surge::Storage::KeyboardMapping Surge::Storage::parseKBMData(const std::string &d)
{
    std::istringstream iss(d);
    auto res = keyboardMappingFromStream(iss);
    res.name = "Mapping from Patch";
    return res;
}

std::ostream& Surge::Storage::operator<<(std::ostream& os, const Surge::Storage::Tone& t)
{
   os << (t.type == Tone::kToneCents ? "cents" : "ratio") << "  ";
   if (t.type == Tone::kToneCents)
      os << t.cents;
   else
      os << t.ratio_n << " / " << t.ratio_d;
   os << "  (f=" << t.floatValue << " c=" << t.cents << ")";
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

bool Surge::Storage::Scale::isValid() const
{
   if (count <= 0)
      return false;

   // TODO check more things maybe...
   return true;
}

std::string Surge::Storage::Scale::toHtml(SurgeStorage *storage)
{
    std::ostringstream htmls;

    htmls << 
        R"HTML(
<html>
  <head>
    <link rel="stylesheet" type="text/css" href="https://fonts.googleapis.com/css?family=Lato" />
    <style>
table {
  border-collapse: collapse;
}

td {
  border: 1px solid #CDCED4;
  padding: 2pt;
}

th {
  padding: 4pt;
  color: #123463;
  background: #CDCED4;
  border: 1px solid #123463;
}
</style>
  </head>
  <body style="margin: 0pt; background: #CDCED4;">
    <div style="border-bottom: 1px solid #123463; background: #ff9000; padding: 2pt;">
      <div style="font-size: 20pt; font-family: Lato; padding: 2pt; color:#123463;">
        Surge Tuning
      </div>
      <div style="font-size: 12pt; font-family: Lato; padding: 2pt;">
    )HTML" 
       << description << 
    R"HTML(
      </div>
    </div>

    <div style="margin:10pt; padding: 5pt; border: 1px solid #123463; background: #fafbff;">
      <div style="font-size: 12pt; margin-bottom: 10pt; font-family: Lato; color: #123463;">
        Tuning Information
     )HTML";

    if( storage && ! storage->isStandardMapping )
    {
       htmls << "<ul>\n"
             << "<li><a href=\"#rawscl\">Jump to Raw SCL</a>.\n"
             << "<li><a href=\"#rawkbm\">Jump to raw KBM</a>.\n"
             << "<li>Scale position 0 maps to key "
             << storage->currentMapping.middleNote << "\n<li> Midi note " << storage->currentMapping.tuningConstantNote << " is set to frequency "
             << storage->currentMapping.tuningFrequency << "hz.\n</ul> ";
    }
    else
    {
       htmls << "\n<div>Tuning uses standard keyboard mapping.\n"
             << "<ul>\n"
             << "<li><a href=\"#rawscl\">Jump to Raw SCL</a>.\n"
             << "</ul>\n";
    }

    htmls << R"HTML(
      </div>
      <div style="font-size: 12pt; font-family: Lato;">
        <div style="padding-bottom: 10pt;">
        )HTML" << count << " tones" <<
R"HTML(
    </div>
        <table>
          <tr>
            <th>#</th><th>Datum</th><th>Cents</th><th>Float</th>
          </tr>
          <tr>
            <td>0</td><td>1</td><td>0</td><td>1</td>
          </tr>
    )HTML";

    int ct = 1;
    for( auto & t : tones )
    {
       htmls << "<tr><td> " << ct++ << "</td><td>";
       if (t.type == Tone::kToneCents)
          htmls << t.cents;
       else
          htmls << t.ratio_n << " / " << t.ratio_d;

       htmls << "</td><td>" << t.cents << "</td><td>" << t.floatValue << "</td></tr>\n";
    };

    htmls << R"HTML(
        </table>

        <p>
)HTML";


       
    htmls << R"HTML(
<p>
        <table>
          <tr>
            <th>Midi Note</th><th>Scale Position</th><th>Frequency</th>
          </tr>

)HTML";

       for( int i=0; i<128; ++i )
       {
          int octave = (i / 12) - 1;
          char notenames[12][3] = {"C ", "C#", "D ", "D#", "E ", "F ", "F#", "G ", "G#", "A ", "A#", "B "};

          htmls << "<tr><td>" << i << " (" << notenames[i % 12 ] << octave << ")</td>\n";

          auto tn = i - storage->scaleConstantNote();
          if( ! storage->isStandardMapping )
          {
             tn = i - storage->currentMapping.middleNote;
          }
          while( tn < 0 ) tn += count;
          
          auto p = storage->note_to_pitch(i);
          htmls << "<td>" << (tn % count + 1) << "</td><td>" << 8.175798915 * p << " hz</td>";
          htmls << "</tr>\n";
       }
       
       htmls << R"HTML(
        </table>
      </div>

    </div>

    <div style="margin:10pt; padding: 5pt; border: 1px solid #123463; background: #fafbff;">
      <div style="font-size: 12pt; font-family: Lato; color: #123463;">
        <a name="rawscl">Tuning Raw File</a>:
           )HTML" << name << "</div>\n<pre>\n" << rawText << R"HTML(
      </pre>
    </div>
)HTML";

       if( ! storage->isStandardMapping )
       {
          htmls << R"HTML(
    <div style="margin:10pt; padding: 5pt; border: 1px solid #123463; background: #fafbff;">
      <div style="font-size: 12pt; font-family: Lato; color: #123463;">
        <a name="rawkbm">Keyboard Mapping Raw File</a>:
           )HTML" << name << "</div>\n<pre>\n" << storage->currentMapping.rawText << R"HTML(
      </pre>
    </div>
)HTML";
       }
       htmls << R"HTML(
  </body>
</html>
      )HTML";
 
  return htmls.str();

}

Surge::Storage::KeyboardMapping Surge::Storage::KeyboardMapping::tuneA69To(double freq)
{
   // There's a couple of ways to do this but since I want it to stream I will syntheitcally create
   // a KBM file
   std::ostringstream oss;
   oss << R"KBM(! Surge Synthetic Keyboard Tuning to Retune A69
!
! Map Size
0
! First note
0
! Last note
127
! First mapping
60
! Reference Note
69
! Reference Freqency
)KBM" << freq << R"KBM(
! Scale Degree
0
! Mapping)KBM";
   std::cout << oss.str() << std::endl;;
   return parseKBMData( oss.str() );
}
