#include <iostream>
#include <iomanip>
#include <sstream>

#include "HeadlessUtils.h"
#include "Player.h"
#include "SurgeError.h"
#include "HeadlessNonTestFunctions.h"
#include "version.h"

#include "Tunings.h"

/*
 * This is a simple main that either routes around or routes to catch2 main.
 * If it routes around, it heads into something in HeadlessNonTestFunctions
 */
int main(int argc, char** argv)
{
   std::cout << "# surge-headless: " << Surge::Build::FullVersionStr
             << " built: " << Surge::Build::BuildDate << " " << Surge::Build::BuildTime << "\n";

   if( argc > 2 && strcmp( argv[1], "--non-test" ) == 0 )
   {
      std::cout << "# Running in non-test mode : " << argv[2] << std::endl;
      try
      {
         if( strcmp( argv[2], "--stats-from-every-patch" ) == 0 )
         {
            Surge::Headless::NonTest::statsFromPlayingEveryPatch();
         }
         if( strcmp( argv[2], "--restream-templates" ) == 0 )
         {
            Surge::Headless::NonTest::restreamTemplatesWithModifications();
         }
         if( strcmp( argv[2], "--generate-nlf-norms" ) == 0 )
         {
            Surge::Headless::NonTest::generateNLFeedbackNorms();
         }
         if( strcmp( argv[2], "--filter-analyzer" ) == 0 )
         {
            if( argc < 4 )
            {
               std::cout << "Usage: --filter-analyzer type subtype\n";
               return 1;
            }
            Surge::Headless::NonTest::filterAnalyzer(std::atoi(argv[3]),
                                                     std::atoi(argv[4]),
                                                     std::cout
            );
         }
      }
      catch( Surge::Error &e )
      {
         std::cout << "SurgeError: " << e.getTitle() << "\n" << e.getMessage() << "\n";
         return 1;
      }

      return 0;
   }
   else
   {
      if( argc > 1 && strcmp( argv[1], "--help" ) == 0 )
      {
         std::cout
             << "It runs in two modes; a regtest mode and a utility mode. To use regtest mode\n"
             << "see the options below. To use utility mode make the first argument '--non-test' and\n"
             << "then use the options below\n\n"
             << "   --non-test --stats-from-every-patch    # play every patch and show RMS\n"
             << "   --non-test --filter-analyzer ft fst    # analyze filter type/subtype for response\n"
             << "\n"
             << "If you exlude the `--non-test` argument, standard catch2 arguments, below, apply\n\n";
      }

      extern int runAllTests(int, char**);
      return runAllTests(argc, argv);
   }
}
