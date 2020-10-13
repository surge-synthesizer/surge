
#define CATCH_CONFIG_RUNNER
#include "catch2/catch2.hpp"

/*
** All moved to UnitTestsBLAH.cpp
*/

int runAllTests(int argc, char **argv)
{
   int result = Catch::Session().run( argc, argv );
   return result;
}
