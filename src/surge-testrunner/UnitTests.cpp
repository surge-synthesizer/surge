/*
 * Surge XT - a free and open source hybrid synthesizer,
 * built by Surge Synth Team
 *
 * Learn more at https://surge-synthesizer.github.io/
 *
 * Copyright 2018-2024, various authors, as described in the GitHub
 * transaction log.
 *
 * Surge XT is released under the GNU General Public Licence v3
 * or later (GPL-3.0-or-later). The license is found in the "LICENSE"
 * file in the root of this repository, or at
 * https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 * Surge was a commercial product from 2004-2018, copyright and ownership
 * held by Claes Johanson at Vember Audio during that period.
 * Claes made Surge open source in September 2018.
 *
 * All source for Surge XT is available at
 * https://github.com/surge-synthesizer/surge
 */

#define CATCH_CONFIG_RUNNER
#include "catch2/catch_amalgamated.hpp"
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
                      << "surge-testrunner assumes you run with CWD as root of the\n"
                      << "Surge XT repo, so that the above local reference loads.\n\n"
                      << "To fix this, run surge-testrunner from Surge XT or \n"
                      << "set the variable SURGE_TEST_WITH_FILE_ERRORS and tests\n"
                      << "will continue." << std::endl;

            if (!getenv("SURGE_TEST_WITH_FILE_ERRORS"))
                return 62;
        }
    }
    catch (const fs::filesystem_error &e)
    {
        std::cerr << "Filesystem Error " << e.what() << std::endl;
        return 172;
    }

    /*auto surge = Surge::Headless::createSurge(44100);
    std::cout << "Created Surge XT with " << surge->storage.datapath << " & "
              << surge->storage.userDataPath << std::endl;
    std::cout << "WT/Patch = " << surge->storage.wt_list.size() << " & "
              << surge->storage.patch_list.size() << std::endl; */
    int result = Catch::Session().run(argc, argv);
    return result;
}
