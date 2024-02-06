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
#ifndef SURGE_SRC_COMMON_VERSION_H
#define SURGE_SRC_COMMON_VERSION_H

namespace Surge
{
struct Build
{
    static const char *MajorVersionStr;
    static const int MajorVersionInt;

    static const char *SubVersionStr;
    static const int SubVersionInt;

    static const char *ReleaseNumberStr;
    static const char *ReleaseStr;

    static const bool IsRelease; // vs nightly
    static const bool IsNightly;

    static const char *GitHash;
    static const char *GitBranch;

    static const char *BuildNumberStr;

    static const char *FullVersionStr;
    static const char *BuildHost;
    static const char *BuildArch;
    static const char *BuildCompiler;

    static const char *BuildLocation; // Local or Pipeline

    static const char *BuildDate;
    static const char *BuildTime;
    static const char *BuildYear;

    // Some features from cmake
    static const char *CMAKE_INSTALL_PREFIX;
};
} // namespace Surge

#define SURGE_VST2_IDENTIFIER 'cjs3'

#define stringProductName "Surge XT"
#define stringOrganization "https://github.com/surge-synthesizer/"
#define stringRepository "https://github.com/surge-synthesizer/surge/"
#define stringWebsite "https://surge-synthesizer.github.io/"
#define stringManual "https://surge-synthesizer.github.io/manual-xt/"
#define stringFileDescription "Surge XT - Hybrid Synthesizer"
#define stringCompanyName "Surge Synth Team\0"
#define stringLegalCopyright "© 2004-2024 Various Authors"
#define stringLegalTrademarks "VST is a trademark of Steinberg Media Technologies GmbH"

#endif //__version__
