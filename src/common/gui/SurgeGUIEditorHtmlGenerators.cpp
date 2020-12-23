#include "SurgeGUIEditor.h"
#include "UserDefaults.h"

namespace Surge
{
namespace UI
{
#include "SkinImageMaps.h"
}
}

std::string SurgeGUIEditor::tuningToHtml()
{
   std::ostringstream htmls;

   htmls <<
      R"HTML(
<html>
  <head>
    <link rel="stylesheet" type="text/css" href="https://fonts.googleapis.com/css?family=Lato" />
    <style>
      table
      {
        border-collapse: collapse;
      }
      
      td, tr
      {
        border: 1px solid #123463;
        padding: 4pt 2pt;
      }
      
      th
      {
        padding: 5pt;
        color: #123463;
        background: #CDCED4;
        border: 1px solid #123463;
      }
      
      .cnt
      {
         text-align: center;
      }
    </style>
  </head>
  <body style="margin: 0pt; background: #CDCED4;">
    <div style="border-bottom: 1px solid #123463; background: #ff9000; padding: 2pt;">
      <div style="font-size: 20pt; font-family: Lato; padding: 2pt; color:#123463;">
        Surge Tuning Information
      </div>
      <div style="font-size: 12pt; font-family: Lato; padding: 2pt;">
    )HTML"
       << synth->storage.currentScale.description <<
    R"HTML(
      </div>
    </div>

    <div style="margin:10pt; padding: 5pt; border: 1px solid #123463; background: #fafbff;">
      <div style="font-size: 12pt; margin-bottom: 10pt; font-family: Lato; color: #123463;">
         )HTML";

        if( ! synth->storage.isStandardMapping )
        {
           htmls << "<ul>\n"
                 << "<li><a href=\"#rawscl\">Raw Scala Tuning (.SCL)</a>\n"
                 << "<li><a href=\"#rawkbm\">Raw Keyboard Mapping (.KBM)</a>\n"
                 << "<li><a href=\"#matrices\">Interval Matrices</a>\n"
                 << "</ul>";
        }
        else
        {
           htmls << "<ul>\n"
                 << "<li><a href=\"#rawscl\">Raw Scala Tuning (.SCL)</a>\n"
                 << "<li><a href=\"#matrices\">Interval Matrices</a>\n"
                 << "</ul>\n";
        }

    htmls << R"HTML(
      </div>
    </div>

    <div style="margin:10pt; padding: 5pt; border: 1px solid #123463; background: #fafbff;">
      <div style="font-size: 12pt; margin-bottom: 10pt; font-family: Lato; color: #123463;">

      <div style="font-size: 12pt; font-family: Lato;">
        <div style="padding-bottom: 10pt;">
    )HTML";

        if (!synth->storage.isStandardMapping)
        {
           htmls << "Scale position 0 maps to MIDI note "
                 << synth->storage.currentMapping.middleNote << "\n</br>"
                 << "MIDI note " << synth->storage.currentMapping.tuningConstantNote << " is set to a frequency of "
                 << synth->storage.currentMapping.tuningFrequency << " Hz.\n</div> ";
        }
        else
        {
           htmls << "\nTuning uses standard keyboard mapping.\n</div>";
        }


        htmls << synth->storage.currentScale.count << " tones\n</p>" <<
R"HTML(
    </div>
        <table>
          <tr>
            <th>#</th><th>Datum</th><th>Float</th><th>Cents</th><th>Cents Interval</th>
          </tr>
          <tr class="cnt">
            <td>0</td><td>1</td><td>1</td><td>0</td><td>-</td>
          </tr>
    )HTML";

    int ct = 1;
    float priorCents = 0;
    for( auto & t : synth->storage.currentScale.tones )
    {
       htmls << "<tr class=\"cnt\"><td> " << ct++ << "</td><td>";
       if (t.type == Tunings::Tone::kToneCents)
          htmls << t.cents;
       else
          htmls << t.ratio_n << " / " << t.ratio_d;
       float interval = t.cents - priorCents;
       priorCents = t.cents;
       htmls << "</td><td>" << t.floatValue << "</td><td>" << t.cents << "</td><td>" << interval << "</td></tr>\n";
    };

    htmls << R"HTML(
        </table>

        <p>
)HTML";



    htmls << R"HTML(
<p>
        <table>
          <tr>
            <th colspan=2>MIDI Note</th><th>Scale Position</th><th>Frequency</th>
          </tr>

)HTML";

       for( int i=0; i<128; ++i )
       {
          int oct_offset = 1;
             oct_offset = Surge::Storage::getUserDefaultValue(&(this->synth->storage), "middleC", 1);
          char notename[16];

          std::string rowstyle="";
          std::string tdopen="<td colspan=2>";
          int np = i%12;
          if( np == 1 || np == 3 || np == 6 || np ==8 || np == 10 )
          {
             rowstyle = "style=\"background-color: #dddddd;\"";
             tdopen="<td style=\"background-color: #ffffff;\">&nbsp;</td><td>";
          }
          htmls << "<tr " << rowstyle << ">" << tdopen << i << " (" << get_notename(notename, i, oct_offset) << ")</td>\n";

          auto tn = i - synth->storage.scaleConstantNote();
          if( ! synth->storage.isStandardMapping )
          {
             tn = i - synth->storage.currentMapping.middleNote;
          }
          while( tn < 0 ) tn += synth->storage.currentScale.count;

          auto p = synth->storage.note_to_pitch(i);
          htmls << "<td class=\"cnt\">" << (tn % synth->storage.currentScale.count + 1) << "</td><td class=\"cnt\">" << 8.175798915 * p << " Hz</td>";
          htmls << "</tr>\n";
       }

       htmls << R"HTML(
        </table>
      </div>

    </div>
    </div>

    <div style="margin:10pt; padding: 5pt; border: 1px solid #123463; background: #fafbff;">
      <div style="font-size: 13pt; font-family: Lato; font-weight: 600; color: #123463;">
        <a name="rawscl">Tuning Raw File</a>:
           )HTML" << synth->storage.currentScale.name << "</div></br>\n<pre>\n" << synth->storage.currentScale.rawText << R"HTML(
      </pre>
    </div>
)HTML";

       if( ! synth->storage.isStandardMapping )
       {
          htmls << R"HTML(
    <div style="margin:10pt; padding: 5pt; border: 1px solid #123463; background: #fafbff;">
      <div style="font-size: 13pt; font-family: Lato; font-weight: 600; color: #123463;">
        <a name="rawkbm">Keyboard Mapping Raw File</a>:
           )HTML" << synth->storage.currentMapping.name << "</div></br>\n<pre>\n" << synth->storage.currentMapping.rawText << R"HTML(
      </pre>
    </div>
)HTML";
       }


       // Interval matrices
       htmls << R"HTML(
    <div style="margin:10pt; padding: 5pt; border: 1px solid #123463; background: #fafbff;">
        <div style="font-size: 13pt; font-family: Lato; font-weight: 600; color: #123463;">
        <a name="matrices">Interval Matrices</a>:
           )HTML" << synth->storage.currentScale.name << "</div></br>\n";

       if( synth->storage.currentMapping.count > 48 )
       {
          htmls << "Surge only displays inverval matrices for scales lower than 48 in length" << std::endl;
       }
       else
       {
          int w = synth->storage.currentScale.count;
          htmls << "<table><tr>";
          for( int i=0; i<=w; ++i )
          {
             htmls << "<th>" << i << "</th>";
          }
          htmls << "</tr></td>";

          // Do a trick of rotating by double filling cents so we don't have to index wrap
          std::vector<float> cents;
          float lastc = 0;
          cents.push_back( 0 );
          for( auto & t : synth->storage.currentScale.tones )
          {
             cents.push_back( t.cents );
             lastc = t.cents;
          }
          for( auto & t : synth->storage.currentScale.tones )
          {
             cents.push_back( t.cents + lastc );
          }


          
          htmls << "<tr><th colspan=\"" << w+1 << "\">Degrees under Rotation</th></tr>";

          for( int rd=0; rd<w; ++rd )
          {
             htmls << "<tr class=\"cnt\"><th>" << rd << "</th>";

             for( int p = 1; p <= w; ++p )
             {
                htmls << "<td>" << std::setprecision(5) <<  cents[p+rd] - cents[rd] << "</td>";
             }
             
             htmls << "</tr>";
          }
          
          htmls << "<tr><th colspan=\"" << w+1 << "\">Intervals under Rotation</th></tr>";

          for( int rd=0; rd<w; ++rd )
          {
             htmls << "<tr class=\"cnt\"><th>" << rd << "</th>";

             for( int p = 1; p <= w; ++p )
             {
                htmls << "<td>" << std::setprecision(5) <<  cents[p+rd] -  cents[ p + rd - 1 ] << "</td>";
             }
             
             htmls << "</tr>";
          }

          htmls << "</table>";
       }
          

       
       htmls << R"HTML(
    </div>
)HTML";
       
       htmls << R"HTML(
  </body>
</html>
      )HTML";

  return htmls.str();
}

std::string SurgeGUIEditor::midiMappingToHtml()
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
  padding: 2pt 4px;
}

.center {
  text-align: center;
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
        Surge MIDI Mapping
      </div>
    </div>

    <div style="margin:10pt; padding: 5pt; border: 1px solid #123463; background: #fafbff;">
      <div style="font-size: 12pt; margin-bottom: 10pt; font-family: Lato; color: #123463;">

     )HTML";
   // TODO - if there are none print differently
   bool foundOne = false;
   int n = n_global_params + n_scene_params;
   for (int i = 0; i < n; i++)
   {
      if (synth->storage.getPatch().param_ptr[i]->midictrl >= 0)
      {
         if( ! foundOne )
         {
            foundOne = true;
            htmls << "Individual parameter MIDI mappings<p>\n"
                  << "<table><tr><th>CC#</th><th>Parameter</th></tr>\n";
         }
         htmls << "<tr><td class=\"center\">" << synth->storage.getPatch().param_ptr[i]->midictrl << "</td><td> "
               << synth->storage.getPatch().param_ptr[i]->get_full_name() << "</td></tr>\n";
      }
   }
   if( foundOne )
   {
      htmls << "</table>\n";
   }
   else
   {
      htmls << "No parameter MIDI mappings present!";
   }


   htmls << R"HTML(

      </div>
    </div>
    <div style="margin:10pt; padding: 5pt; border: 1px solid #123463; background: #fafbff;">
      <div style="font-size: 12pt; margin-bottom: 10pt; font-family: Lato; color: #123463;">
         Macro Assignments<p>
         <table><tr><th>CC#</th><th>Macro</th><th>Custom Name</th></tr>
     )HTML";
   for( int i=0; i<n_customcontrollers; ++i )
   {
      std::string name = synth->storage.getPatch().CustomControllerLabel[i];
      auto ccval = synth->storage.controllers[i];

      htmls << "<tr><td class=\"center\">" << (ccval == -1 ? "N/A" : std::to_string(ccval)) << "</td><td class=\"center\">" << i + 1 << "</td><td>" << name << "</td></tr>" << std::endl;
   }
   htmls << R"HTML(
         </table>
      </div>
    </div>
  </body>
</html>
      )HTML";

  return htmls.str();
}

std::string SurgeGUIEditor::skinInspectorHtml(SkinInspectorFlags f)
{
   std::ostringstream htmls;

   auto startSection = [&htmls]( std::string secname, std::string seclabel ) {
      htmls << R"HTML(
     <div style="margin:10pt; padding: 5pt; border: 1px solid #123463; background: #fafbff;">
                <div style="font-size: 12pt; margin-bottom: 10pt; font-family: Lato; color: #123463;">

          )HTML" << "<a name=\"" << secname << "\"><h2>" << seclabel << "</h2></a>\n";
   };
   auto endSection = [&htmls]() {
     htmls << R"HTML(
        </div></div>
          )HTML";
   };
   htmls << R"HTML(
   <html>
   <head>
   <link rel="stylesheet" type="text/css" href="https://fonts.googleapis.com/css?family=Lato" />
                                               <style>
                                               table {
                                                   border-collapse: collapse;
                                               }

   td {
       border: 1px solid #CDCED4;
       padding: 2pt 4px;
   }

       .center {
      text-align: center;
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
                                         Surge Skin Inspector
                   </div>
                     </div>


    <div style="margin:10pt; padding: 5pt; border: 1px solid #123463; background: #fafbff;">
      <div style="font-size: 12pt; margin-bottom: 10pt; font-family: Lato; color: #123463;">
        <ul>
          <li><a href="#colors">Color Database</a>
          <li><a href="#imageid">Image IDs</a>
          <li><a href="#skinConnectors">Skin Component Connectors</a>
          <li><a href="#loadedImages">Runtime Loaded Images</a>
        </ul>
      </div>
    </div>

   )HTML";

   startSection("colors", "Color Database" );
   htmls << "<table><tr><th>Color Name</th><th colspan=2>Default Color</th><th colspan=2>Current Color</th></tr>\n";
   auto cols = Surge::Skin::Color::getAllColors();

   auto htmlBlob = [](int r, int g, int b, int a )
   {
      std::ostringstream rs;
      // rs << "rgba(" << r << "," << g << "," << b << "," << a << ") / ";
      rs << "#" << std::hex << std::setw(2) << std::setfill('0') << r
         << std::setw(2) << std::setfill('0') << g
         << std::setw(2) << std::setfill('0')<< b;
      if( a != 255 ) rs << std::setw(2) << std::setfill('0') << a;
      auto colh= rs.str();

      std::ostringstream cells;
      cells << "<td>" << colh << "</td><td width=10 style=\"background-color: " << colh << "\">&nbsp;</td>";
      return cells.str();
   };

   for( auto &c : cols )
   {
      auto skincol = currentSkin->getColor(c);
      htmls << "<tr><td>" << c.name << "</td>" << htmlBlob( c.r, c.g, c.b, c.a )  << htmlBlob( skincol.red, skincol.green, skincol.blue, skincol.alpha ) << "</tr>\n";
   }
   htmls << "</table>";
   endSection();

   startSection( "imageid", "Image IDs" );

   {
      auto q = Surge::UI::createIdNameMap();

      std::vector<std::pair<std::string, int>> asV(q.begin(), q.end());
      std::sort( asV.begin(), asV.end(), [](auto a, auto b ) { return a.second < b.second; });
      htmls << "<table><tr><th>Resource Number</th><th>Resource ID</th></tr>\n";
      for( auto v : asV )
      {
         htmls << "<tr><td>" << v.second << "</td><td>" << v.first << "</td></tr>\n";
      }
      htmls << "</table>\n";

   }
   endSection();

   htmls <<  R"HTML(
  </body>
</html>
      )HTML";

   // Skin Connectors
   startSection( "skinConnectors", "Skin Component Connectors" );
   {
      auto imgid = Surge::UI::createIdNameMap();

      auto co = Surge::Skin::Connector::allConnectorIDs();
      htmls << "<table><tr><th>Name</th><th>Geometry (XxY+WxH)</th><th>Class</th><th>Properties</th></tr>\n";
      for( auto c : co )
      {
         htmls << "<tr><td>" << c << "</td><td>";
         auto comp = currentSkin->getOrCreateControlForConnector(Surge::Skin::Connector::connectorByID(c));
         if( comp )
         {
            htmls << comp->x << "x" << comp->y;
            if (comp->w > 0 || comp->h > 0)
               htmls << "+" << comp->w << "x" << comp->h;
         }

         htmls << "</td><td>";
         if( comp )
         {
            if( comp->classname == comp->ultimateparentclassname )
               htmls << comp->classname;
            else
               htmls << comp->classname << " : " << comp->ultimateparentclassname;
         }

         htmls << "</td><td>";
         if( comp )
            for( auto p: comp->allprops )
            {
               if( p.first != "x" && p.first != "y" && p.first != "w" && p.first != "h" )
               {
                  htmls << p.first << ": " << p.second;
                  if( p.first == "bg_id" )
                  {
                     int id = std::atoi(p.second.c_str());
                     // Linear search kinda sucks but this is debugging code for UI generation on small list
                     for( auto q : imgid )
                        if( q.second == id )
                           htmls << " (" << q.first << ")\n";
                  }
                  htmls << "<br>\n";
               }
            }
         htmls << "</td></tr>\n";
      }
      htmls << "</table>\n";
   }
   endSection();

   // Loaded Images
   startSection( "loadedImages", "Runtime Loaded Images" );
   {
      htmls << "<ul>\n";
      auto r1 = bitmapStore->nonResourceBitmapIDs(SurgeBitmaps::STRINGID);
      for( auto v : r1 )
      {
         htmls << "<li>" << v << "</li>\n";
      }
      auto r2 = bitmapStore->nonResourceBitmapIDs(SurgeBitmaps::PATH);
      for( auto v : r2 )
      {
         htmls << "<li>" << v << "</li>\n";
      }
      htmls << "</ul>\n";
   }
   endSection();
   return htmls.str();
}