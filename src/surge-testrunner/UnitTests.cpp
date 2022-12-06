
#define CATCH_CONFIG_RUNNER
#include "catch2/catch2.hpp"
#include "HeadlessUtils.h"

/*
** All moved to UnitTestsBLAH.cpp
*/

int runAllTests(int argc, char **argv)
{
    try
    {
        std::string tfn = "resources/test-data/wav/Wavetable.wav";
        auto p = fs::path{tfn};
        if (!fs::exists(p))
        {
            std::cout << "Can't find file '" << tfn << "'.\n"
                      << "\n"
                      << "This means several tests will fail. Currently the\n"
                      << "test runner assumes you run with CWD as root of the\n"
                      << "surge repo so that the above local reference loads.\n\n"
                      << "To fix this, run the test runner from surge or \n"
                      << "set the variable SURGE_TEST_WITH_FILE_ERRORS and tests\n"
                      << "will continue." << std::endl;

            if (!getenv("SURGE_TEST_WITH_FILE_ERRORS"))
                return 62;
        }
    }
    catch (const fs::filesystem_error &e)
    {
        std::cerr << "FileSystem Error " << e.what() << std::endl;
        return 172;
    }

    auto surge = Surge::Headless::createSurge(44100);
    std::cout << "Created surge with " << surge->storage.datapath << " & "
              << surge->storage.userDataPath << std::endl;
    std::cout << "WT/Patch = " << surge->storage.wt_list.size() << " & "
              << surge->storage.patch_list.size() << std::endl;
    int result = Catch::Session().run(argc, argv);
    return result;
}
