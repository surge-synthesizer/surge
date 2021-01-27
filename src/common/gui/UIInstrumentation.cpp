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

void record(std::string tag) { records[tag]++; }
void report()
{
    std::ostringstream oss;
    oss << "<html><body>\n";
    oss << "<h1>Recorded events</h1><table border=2>\n\n";

    std::map<std::string, int> sr;
    for (auto &k : records)
        sr[k.first] = k.second;

    for (auto &k : sr)
    {
        oss << "<tr><td>" << k.first << " </td><td> " << k.second << "</td></tr>\n" << std::endl;
    }
    oss << "</table></body></html>\n";
    Surge::UserInteractions::showHTML(oss.str());
}

TimeThisBlock::TimeThisBlock(std::string i) : tag(i)
{
    start = std::chrono::high_resolution_clock::now();
}

TimeThisBlock::~TimeThisBlock()
{
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = end - start;
    auto durationFloat = duration.count();
    // std::cout << "Block in " << tag << " took " << durationFloat << std::endl;
    // FIX - record this so I can report
}
} // namespace Debug
} // namespace Surge
#endif
