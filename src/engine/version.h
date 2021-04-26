#ifndef __version__
#define __version__

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

#define stringProductName "Surge"
#define stringWebsite "https://surge-synthesizer.github.io/"
#define stringFileDescription "Surge Synth Team Surge"
#define stringCompanyName "Surge Synth Team\0"
#define stringLegalCopyright "© 2004-2020 Various Authors"
#define stringLegalTrademarks "VST is a trademark of Steinberg Media Technologies GmbH"

#endif //__version__
