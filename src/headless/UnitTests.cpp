
#define CATCH_CONFIG_RUNNER
#include "catch2/catch2.hpp"
#include "HeadlessUtils.h"

/*
** All moved to UnitTestsBLAH.cpp
*/

int runAllTests(int argc, char **argv)
{
    auto surge = Surge::Headless::createSurge(44100);
    std::cout << "Created surge with " << surge->storage.datapath << " & "
              << surge->storage.userDataPath << std::endl;
    std::cout << "WT/Patch = " << surge->storage.wt_list.size() << " & "
              << surge->storage.patch_list.size() << std::endl;
    int result = Catch::Session().run(argc, argv);
    return result;
}
