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
#include <numeric>
#include <cmath>
#include <algorithm>

#ifdef INSTRUMENT_UI
namespace Surge
{
namespace Debug
{
   std::unordered_map<std::string, std::atomic<int>> records;
   std::unordered_map<std::string, std::vector<long>> times;
   
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
      oss << "</table><h1>timed events (microseconds)</h1><table border=2>\n\n";

      std::map<std::string, std::vector<long>> st;
      for( auto &t : times )
         st[t.first] = t.second;
      
      for( auto &t : st )
      {
         auto tv = t.second;
         double sum = std::accumulate(tv.begin(), tv.end(), 0.0);
         double mean = sum / tv.size();
         const auto minmaxres = std::minmax_element(tv.begin(), tv.end());

         
         double sq_sum = std::inner_product(tv.begin(), tv.end(), tv.begin(), 0.0);
         double stdev = std::sqrt(sq_sum / tv.size() - mean * mean);
         oss << "<tr><td>" << t.first << "</td><td>mean=" << mean << "</td><td>stdev=" << stdev
                   << "</td><td>min=" << *(minmaxres.first) << "</td><td>max="<< *(minmaxres.second) << "</td></tr>" << std::endl;
      }
      oss << "</table></body></html>\n";
      
      std::cout << oss.str() << std::endl;
      Surge::UserInteractions::showHTML( oss.str() );
   }

   TimeThisBlock::TimeThisBlock(std::string i ) : tag( i ) {
      start = std::chrono::high_resolution_clock::now();
   }

   TimeThisBlock::~TimeThisBlock() {
      auto end = std::chrono::high_resolution_clock::now();
      auto dur = end - start;
      auto durus = std::chrono::duration_cast<std::chrono::microseconds>(dur).count();
      times[tag].push_back( durus );
      // std::cout << "Block in " << tag << " took " << durationFloat << std::endl;
      // FIX - record this so I can report
   }
}
}
#endif
