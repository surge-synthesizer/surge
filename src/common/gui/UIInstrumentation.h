// -*-c++-*-
#pragma once
#include <string>
#include <chrono>

// #define INSTRUMENT_UI

#ifdef INSTRUMENT_UI
namespace Surge {
namespace Debug {
   /*
   ** Just count how many times we hit this function 
   */
   void record( std::string tag );

   /*
   ** Pop up the timing report in a browser 
   */
   void report();

   /*
   ** A little structure which records the time for a block. Like
   ** {
   **    Surge::Debug::TimeThisBlock( "howlong" )
   **    // do something slow
   ** }
   */
   struct TimeThisBlock {
      TimeThisBlock( std::string tag );
      ~TimeThisBlock();
      std::string tag;
      std::chrono::time_point<std::chrono::high_resolution_clock> start;
   };
};
};
#endif
