// -*-c++-*-
#pragma once
#include <string>

#define INSTRUMENT_UI

#ifdef INSTRUMENT_UI
namespace Surge {
namespace Debug {
   void record( std::string tag );
   void report();
};
};
#endif
