#include "UIInstrumentation.h"
#include "UserInteractions.h"
#include <unordered_map>
#include <map>
#include <vector>
#include <string>
#include <atomic>
#include <sstream>
#include <iostream>
#include <iomanip>

#ifdef INSTRUMENT_UI
namespace Surge
{
namespace Debug
{
   std::unordered_map<std::string, std::atomic<int>> records;
   
   void record( std::string tag ) {
      records[tag]++;
   }
   void report() {
      std::ostringstream oss;
      oss << "<html><body>\n";
      oss << "<h1>Recorded events</h1><table border=2>\n\n";

      std::map<std::string, int> sr;
      for( auto &k : records )
         sr[k.first] = k.second;
      
      for( auto &k : sr )
      {
         oss << "<tr><td>" << k.first << " </td><td> " << k.second << "</td></tr>\n" << std::endl;
      }
      oss << "</table></body></html>\n";
      Surge::UserInteractions::showHTML( oss.str() );
   }
}
}
#endif
