
solution "Surge"
configurations { "Debug", "Release" }

language "C++"

-- GLOBAL STUFF --

local VSTGUI_DIR = os.getenv("VSTGUI_DIR")

if VSTGUI_DIR then
   VSTGUI = VSTGUI_DIR .. "/vstgui/"
else
   VSTGUI = "vstgui.surge/vstgui/"
   -- VSTGUI = "vst3sdk/vstgui4/vstgui/";
end

defines
{
    "VSTGUI_ENABLE_DEPRECATED_METHODS=0",
    "SURGE_VERSION=" .. string.gsub(io.readfile("VERSION"), "\n$", "")
}

floatingpoint "Fast"

configuration { "Debug" }
defines { "DEBUG=1", "RELEASE=0" }
symbols "On"
targetdir "target/Debug"

configuration { "Release*" }
defines { "DEBUG=0", "RELEASE=1" }
optimize "Speed"
targetdir "target/Release"

configuration {}

if (os.istarget("macosx")) then

    flags { "StaticRuntime" }
    vectorextensions "SSE2"

    defines
    {
        "_aligned_malloc(x,a)=malloc(x)",
        "_aligned_free(x)=free(x)",
        "MAC_COCOA=1",
        "COCOA=1",
        "OBJC_OLD_DISPATCH_PROTOTYPES=1"
    }

    links
    {
    }

    defines { "MAC=1" }

    buildoptions
        {
            "-std=c++17", "-stdlib=libc++",
        "-ferror-limit=0",
            "-DOBJC_OLD_DISPATCH_PROTOTYPES=1",
            "-Wno-deprecated-declarations",        -- Alas the AU V2 uses a whole bunch of deprecated stuff
        "-Wno-inconsistent-missing-override"   -- Surge was written before this was even a keyword! We do need to fix this though
        }
    links { "c++" }
    buildoptions { "-mmacosx-version-min=10.9" }
    linkoptions { "-mmacosx-version-min=10.9" }

    platforms { "x64" }

elseif (os.istarget("linux")) then

    flags { "StaticRuntime" }
    vectorextensions "SSE2"

    defines
    {
        "LINUX=1",
        "_aligned_malloc(x,a)=malloc(x)",
        "_aligned_free(x)=free(x)",
    }

    buildoptions { "-std=c++17" }

    platforms { "x64" }

    files
    {
        "src/linux/ConfigurationXml.S",
        "src/linux/ScalablePiggy.S",
    }

    includedirs
    {
        "src/linux"
    }

    prebuildcommands
    {
        "python scripts/linux/emit-vector-piggy.py"
    }

elseif (os.istarget("windows")) then

    toolset "v141"
    defines
    {
        "WINDOWS=1",
        "WIN32",
        "_WINDOWS",
        "_WIN32_WINNT=0x0601",
        "_USRDLL",
        "VA_SUBTRACTIVE_EXPORTS",
        "TIXML_USE_STL",
        "USE_LIBPNG",
        "_CRT_SECURE_NO_WARNINGS",
        "NOMINMAX=1" -- Jan 2019 update to vst3sdk required this to disambiguoaute std::numeric_limits. See #373
    }

    characterset "MBCS"

    buildoptions { "/MP /Zc:alignedNew /std:c++17 /bigobj" }

    includedirs {
        "libs/wtl",
        "libs/libpng/win/include"
    }

    flags { "StaticRuntime", "NoMinimalRebuild" }

    platforms { "x64", "x86" }

    filter "platforms:x86"
        targetsuffix "32"
    filter "platforms:x64"
        targetsuffix ""
    filter {}

    configuration { "Debug" }
    links {
        "libs/libpng/win/x64/lib/libpng16_staticd.lib"
    }
    configuration { "Release*" }
    links {
        "libs/libpng/win/x64/lib/libpng16_static.lib"
    }

    configuration {}
end

includedirs {
    "libs/xml",
    "src/common/vt_dsp",
    "src/common/thread",
    "vst3sdk/vstgui4",
    "vst3sdk",
    "libs/"
    }

-- PLUGIN COMMON --

function plugincommon()

    targetprefix ""
    targetname "Surge"

    files {
        "src/common/dsp/effect/ConditionerEffect.cpp",
        "src/common/dsp/effect/DistortionEffect.cpp",
        "src/common/dsp/effect/DualDelayEffect.cpp",
        "src/common/dsp/effect/Effect.cpp",
        "src/common/dsp/effect/FreqshiftEffect.cpp",
        "src/common/dsp/effect/PhaserEffect.cpp",
        "src/common/dsp/effect/Reverb1Effect.cpp",
        "src/common/dsp/effect/Reverb2Effect.cpp",
        "src/common/dsp/effect/RotarySpeakerEffect.cpp",
        "src/common/dsp/effect/VocoderEffect.cpp",
        "src/common/dsp/AdsrEnvelope.cpp",
        "src/common/dsp/BiquadFilter.cpp",
        "src/common/dsp/BiquadFilterSSE2.cpp",
        "src/common/dsp/DspUtilities.cpp",
        "src/common/dsp/FilterCoefficientMaker.cpp",
        "src/common/dsp/FMOscillator.cpp",
        "src/common/dsp/LfoModulationSource.cpp",
        "src/common/dsp/Oscillator.cpp",
        "src/common/dsp/QuadFilterChain.cpp",
        "src/common/dsp/QuadFilterUnit.cpp",
        "src/common/dsp/SampleAndHoldOscillator.cpp",
        "src/common/dsp/SurgeSuperOscillator.cpp",
        "src/common/dsp/SurgeVoice.cpp",
        "src/common/dsp/VectorizedSvfFilter.cpp",
        "src/common/dsp/Wavetable.cpp",
        "src/common/dsp/WavetableOscillator.cpp",
        "src/common/dsp/WindowOscillator.cpp",
        "src/common/gui/CAboutBox.cpp",
        "src/common/gui/CCursorHidingControl.cpp",
        "src/common/gui/CDIBitmap.cpp",
        "src/common/gui/CEffectSettings.cpp",
        "src/common/gui/CHSwitch2.cpp",
        "src/common/gui/CLFOGui.cpp",
        "src/common/gui/CModulationSourceButton.cpp",
        "src/common/gui/CNumberField.cpp",
        "src/common/gui/COscillatorDisplay.cpp",
        "src/common/gui/CPatchBrowser.cpp",
        "src/common/gui/CScalableBitmap.cpp",
        "src/common/gui/CSnapshotMenu.cpp",
        "src/common/gui/CSurgeSlider.cpp",
        "src/common/gui/CSurgeVuMeter.cpp",
        "src/common/gui/CSwitchControl.cpp",
        "src/common/gui/PopupEditorDialog.cpp",
        "src/common/gui/SurgeBitmaps.cpp",
        "src/common/gui/SurgeGUIEditor.cpp",
        "src/common/thread/CriticalSection.cpp",
        "src/common/util/FpuState.cpp",
        "src/common/vt_dsp/basic_dsp.cpp",
        "src/common/vt_dsp/halfratefilter.cpp",
        "src/common/vt_dsp/lipol.cpp",
        "src/common/vt_dsp/macspecific.cpp",
        "src/common/Parameter.cpp",
        "src/common/precompiled.cpp",
        "src/common/Sample.cpp",
        "src/common/SampleLoadRiffWave.cpp",
        "src/common/SurgeError.cpp",
        "src/common/SurgePatch.cpp",
        "src/common/SurgeStorage.cpp",
        "src/common/SurgeStorageLoadWavetable.cpp",
        "src/common/SurgeSynthesizer.cpp",
        "src/common/SurgeSynthesizerIO.cpp",
        "src/common/UserDefaults.cpp",
        "libs/xml/tinyxml.cpp",
        "libs/xml/tinyxmlerror.cpp",
        "libs/xml/tinyxmlparser.cpp",
        "libs/filesystem/filesystem.cpp",
    }

    includedirs {
        "src/common",
        "src/common/dsp",
        "src/common/gui",
    }

    if (os.istarget("macosx")) then

        pchheader "src/common/precompiled.h"
        pchsource "src/common/precompiled.cpp"

        buildoptions {
            "-Wno-unused-variable"
        }

        sysincludedirs {
            "src/**",
            "libs/**",
            "vst3sdk/vstgui4",
        }

        files
        {
            "src/mac/**.mm",
            "src/mac/**.cpp",
            "src/mac/**.h",
            "libs/vst/*.mm",
            VSTGUI .. "vstgui_mac.mm",
            VSTGUI .. "vstgui_uidescription_mac.mm",
        }

        excludes {
            VSTGUI .. "winfileselector.cpp",
            VSTGUI .. "vstgui_ios.mm",
            VSTGUI .. "vstgui_uidescription_ios.mm",
        }

        includedirs
        {
            "src/mac"
        }

        links {
            "Accelerate.framework",
            "ApplicationServices.framework",
            "AudioUnit.framework",
            "Carbon.framework",
            "CoreAudio.framework",
            "CoreAudioKit.framework",
            "CoreServices.framework",
            "CoreText.framework",
            "Cocoa.framework",
            "CoreFoundation.framework",
            "OpenGL.framework",
            "QuartzCore.framework",
        }

    elseif (os.istarget("linux")) then

        -- pchheader "src/common/precompiled.h" --
        -- pchsource "src/common/precompiled.cpp" --

        buildoptions {
            "-Wno-unused-variable",
            "`pkg-config --cflags cairo fontconfig freetype2 xkbcommon-x11 xcb-cursor xcb-keysyms xcb-xkb xcb-util x11`",
            "-std=c++14"
        }

        configuration { "Release*" }
        buildoptions {
            "-fvisibility=hidden",
            "-fvisibility-inlines-hidden",
            "-fdata-sections",
            "-ffunction-sections",
        }
        configuration {}

        files
        {
            "src/linux/*.mm",
            "src/linux/**.cpp",
            "src/linux/**.h",

            VSTGUI .. "vstgui.cpp",
            VSTGUI .. "lib/platform/linux/**.cpp",
            VSTGUI .. "lib/platform/common/genericoptionmenu.cpp",
            VSTGUI .. "lib/platform/common/generictextedit.cpp",

        }

        excludes {
            VSTGUI .. "winfileselector.cpp",
            VSTGUI .. "vstgui_ios.mm",
            VSTGUI .. "vstgui_uidescription_ios.mm",
        }

        includedirs
        {

        }

        links {
            "pthread",
            "stdc++fs",
            "gcc_s",
            "gcc",
            "dl",
        }

        linkoptions {
            "`pkg-config --libs cairo fontconfig freetype2 xkbcommon-x11 xcb-cursor xcb-keysyms xcb-xkb xcb-util x11`",
            "-Wl,--no-undefined",
        }

        configuration {"Release*"}
        linkoptions {
            "-fdata-sections",
            "-ffunction-sections",
            "-Wl,-O1",
            "-Wl,--as-needed",
            "-Wl,--gc-sections",
            "-Wl,--strip-all",
        }
        configuration{}
    elseif (os.istarget("windows")) then

        pchheader "precompiled.h"
        pchsource "src/common/precompiled.cpp"
        buildoptions { "/FI precompiled.h", "/wd\"4244\"" }

        files
        {
            "src/windows/**.cpp",
            "src/windows/**.h",
            "src/windows/surge.rc",
            "src/windows/scalableui.rc",
            "resources/bitmaps/*.png",
            "assets/original-vector/exported/*.png",
            "resources/fonts/**.ttf",
            VSTGUI .. "vstgui_win32.cpp",
            VSTGUI .. "vstgui_uidescription_win32.cpp",
        }

        includedirs {
            "src/windows",
        }

        linkoptions {
            "/ignore:4996,4244,4267"
        }

        links
        {
            "Winmm",
            "gdi32",
            "gdiplus",
            "ComDlg32",
            "ComCtl32",
            "shell32",
            "user32",
        }

    end
end

function xcodebrewbuildsettings()
    xcodebuildsettings
    {
        ["CC"] = "/usr/local/opt/llvm/bin/clang";
        ["CLANG_LINK_OBJC_RUNTIME"] = "NO";
        ["COMPILER_INDEX_STORE_ENABLE"] = "NO";
    }
end

-- VST2 PLUGIN --

local VST24SDK = os.getenv("VST2SDK_DIR")
local BREWBUILD = os.getenv("BREWBUILD")

if VST24SDK then

    project "surge-vst2"
    kind "SharedLib"
    uuid "007990D5-2B46-481D-B38C-D83037CDF54B"

    defines
    {
        "TARGET_VST2=1",
    }

    plugincommon()

    files {
        "src/vst2/**.cpp",
        "src/vst2/**.h",
        VST24SDK .. "/public.sdk/source/vst2.x/audioeffect.cpp",
        VST24SDK .. "/public.sdk/source/vst2.x/audioeffectx.cpp",
        VST24SDK .. "/public.sdk/source/vst2.x/vstplugmain.cpp",
--      "vst3sdk/public.sdk/source/vst/vst2wrapper/vst2wrapper.cpp",
--      "vst3sdk/public.sdk/source/vst/vst2wrapper/vst2wrapper.sdk.cpp",
        VSTGUI .. "plugin-bindings/aeffguieditor.cpp",
    }

    excludes {
        VSTGUI .. "plugguieditor.cpp",
    }

    includedirs {
        "src/vst2",
        VST24SDK,
    }

    sysincludedirs {
        VST24SDK,
    }

    configuration { "Debug" }
    targetdir "target/vst2/Debug"
    targetsuffix "-Debug"

    configuration { "Release" }
    targetdir "target/vst2/Release"

    configuration {}

    if (os.istarget("macosx")) then

        targetname "Surge"
        targetprefix ""
        postbuildcommands { "./scripts/macOS/package-vst.sh" }

        files
        {
        "libs/vst/*.mm"
        }

        if BREWBUILD then
            xcodebrewbuildsettings()
        end

    elseif (os.istarget("windows")) then

        linkoptions { "/DEF:resources\\windows-vst2\\surge.def" }

    elseif (os.istarget("linux")) then
       -- Ideally we wouldn't define __cdecl on linux. It's not needed
       -- Alas, the VST2 SDK has in aeffgui a _GNUC define which requires
       -- it so without __cdecl blank defined on linux vst2, we don't get
       -- reliable builds. We considered defining it just where used but
       -- it is used also, indirectly, by the vst3 sdk, so the smallest
       -- change is this diff plus this comment.
        defines {
            "__cdecl=",
        }
    end
end

-- VST3 PLUGIN --

project "surge-vst3"
kind "SharedLib"
uuid "A87FBED5-3E7A-432F-8611-B2C61F40F4B8"

defines
{
    "TARGET_VST3=1"
}

plugincommon()

files {
    "src/vst3/**.cpp",
    "src/vst3/**.h",
    "vst3sdk/*.cpp",
    "vst3sdk/base/source/*.cpp",
    "vst3sdk/base/thread/source/*.cpp",
    "vst3sdk/public.sdk/source/common/*.cpp",
    "vst3sdk/public.sdk/source/main/pluginfactoryvst3.cpp",
    "vst3sdk/public.sdk/source/vst/vstguieditor.cpp",
    "vst3sdk/public.sdk/source/vst/vstinitiids.cpp",
    "vst3sdk/public.sdk/source/vst/vstnoteexpressiontypes.cpp",
    "vst3sdk/public.sdk/source/vst/vstsinglecomponenteffect.cpp",
    "vst3sdk/public.sdk/source/vst/vstaudioeffect.cpp",
    "vst3sdk/public.sdk/source/vst/vstcomponent.cpp",
    "vst3sdk/public.sdk/source/vst/vstsinglecomponenteffect.cpp",
    "vst3sdk/public.sdk/source/vst/vstcomponentbase.cpp",
    "vst3sdk/public.sdk/source/vst/vstbus.cpp",
    "vst3sdk/public.sdk/source/vst/vstparameters.cpp",
    "vst3sdk/pluginterfaces/base/*.cpp",
    }

excludes {
    VSTGUI .. "aeffguieditor.cpp",
    "src/vst3/SurgeVst3EditController.*"
}

includedirs {
    "src/vst3",
    "vst3sdk"
}

configuration { "Debug" }
targetdir "target/vst3/Debug"
targetsuffix "-Debug"

configuration { "Release" }
targetdir "target/vst3/Release"

configuration {}

if (os.istarget("macosx")) then

    postbuildcommands { "./scripts/macOS/package-vst3.sh" }

    files
    {
        "vst3sdk/public.sdk/source/main/macmain.cpp",
        "vst3sdk/*.mm"
    }

    if BREWBUILD then
        xcodebrewbuildsettings()
    end

elseif (os.istarget("windows")) then

    linkoptions { "/DEF:resources\\windows-vst3\\surge.def" }

    files
    {
        "vst3sdk/public.sdk/source/main/dllmain.cpp",
    }

    flags { "NoImportLib" }
    targetextension ".vst3"

elseif (os.istarget("linux")) then
    postbuildcommands { "./scripts/linux/package-vst3.sh" }

    files
    {
        "vst3sdk/public.sdk/source/main/linuxmain.cpp",
    }

    excludes {
        -- This is both deprecated and, on linux, ejects a non-linkable symbol. So exclude it.
        "vst3sdk/base/source/timer.cpp"
    }


end

-- AUDIO UNIT PLUGIN --

if (os.istarget("macosx")) then

    project "surge-au"
    kind "SharedLib"
    uuid "f6acbb60-3601-11e4-8c21-0800200c9a66"

    plugincommon()

    configuration { "Debug" }
    targetdir "target/au/Debug"
    targetsuffix "-Debug"

    configuration { "Release" }
    targetdir "target/au/Release"

    configuration {}

    defines
    {
        "TARGET_AUDIOUNIT=1",
        "PLUGGUI=1",
    }

    links
    {
        "AudioToolbox.framework",
        "AudioUnit.framework",
    }

    files
    {
        "src/au/**.cpp",
        "src/au/**.mm",
        "src/au/**.h",
        "libs/AUPublic/**.cpp",
        "libs/AUPublic/**.h",
        "libs/PublicUtility/*.cpp",
        VSTGUI .. "plugin-bindings/plugguieditor.cpp",
    }

    excludes
    {
        "libs/AudioUnits/AUPublic/AUCarbonViewBase/**",
        "libs/AudioUnits/AUPublic/OtherBases/AUPannerBase.*",
        "libs/AudioUnits/PublicUtility/CASpectralProcessor.cpp",
    }

    includedirs
    {
        "src/au",
        "libs/",
        "libs/AudioUnits/AUPublic",
        "libs/AudioUnits/PublicUtility",
    }

    excludes
    {
        VSTGUI .. "winfileselector.cpp",
    }

    postbuildcommands { "./scripts/macOS/package-au.sh" }

    if BREWBUILD then
        xcodebrewbuildsettings()
    end

end

if (os.istarget("linux")) then
    project "surge-app"
    kind "WindowedApp"

    defines
    {
        "TARGET_APP=1"
    }

    plugincommon()

    files {
        "src/app/main.cpp",
        "src/app/PluginLayer.cpp",
        VSTGUI .. "plugin-bindings/plugguieditor.cpp",
    }

    includedirs {
        "src/app"
    }

    links {
        "dl",
        "freetype",
        "fontconfig",
        "X11",
    }

    configuration { "Debug" }
    targetdir "target/app/Debug"
    targetsuffix "-Debug"

    configuration { "Release" }
    targetdir "target/app/Release"
end

-- HEADLESS APP

project "surge-headless"
kind "ConsoleApp"

defines
{
   "TARGET_HEADLESS=1"
}

plugincommon()

files
{
   "src/headless/main.cpp",
   "src/headless/DisplayInfoHeadless.cpp",
   "src/headless/UserInteractionsHeadless.cpp",
   "src/headless/LinkFixesHeadless.cpp"
}

excludes
{
   "src/common/gui/*"
}

includedirs
{
   "src/headless"
}

configuration { "Debug" }
targetdir "target/headless/Debug"
targetsuffix "-Debug"

configuration { "Release" }
targetdir "target/headless/Release"

configuration {}

if (os.istarget("macosx")) then
   excludes
   {
      VSTGUI .. "vstgui_mac.mm",
      VSTGUI .. "vstgui_uidescription_mac.mm",
      "src/mac/DisplayInfoMac.mm",
      "src/mac/UserInteractionsMac.cpp",
      "src/mac/cocoa_utils.mm",
      "src/mac/RuntimeFontMac.cpp"
   }
end

if (os.istarget("windows")) then
   excludes
   {
      VSTGUI .. "vstgui_win32.cpp",
      VSTGUI .. "vstgui_uidescription_win32.cpp",
      "src/windows/DisplayInfoWin.cpp",
      "src/windows/UserInteractionsWin.cpp",
      "src/windows/RuntimeFontWin.cpp"
   }
end

if (os.istarget("linux")) then
   excludes
   {
      VSTGUI .. "vstgui.cpp",
      VSTGUI .. "lib/platform/linux/**.cpp",
      VSTGUI .. "lib/platform/common/genericoptionmenu.cpp",
      VSTGUI .. "lib/platform/common/generictextedit.cpp",
      "src/linux/DisplayInfoLinux.cpp",
      "src/linux/UserInteractionsLinux.cpp",
   }
end
