
solution "Surge"
configurations { "Debug", "Release" }

language "C++"

-- GLOBAL STUFF --

VSTGUI = "vst3sdk/vstgui4/vstgui/";

defines
{ 
  "VSTGUI_ENABLE_DEPRECATED_METHODS=0"
}

floatingpoint "Fast"

configuration { "Debug" }
defines { "DEBUG=1", "RELEASE=0" }
targetdir "target/Debug"

configuration { "Release*" }
defines { "DEBUG=0", "RELEASE=1" }
optimize "Speed"
symbols "On"
targetdir "target/Release"

configuration {}

if (os.istarget("macosx")) then

	flags { "EnableSSE2", "StaticRuntime" }
	
	defines
	{ 
		"PPC=0",
		"_MM_ALIGN16=__attribute__((aligned(16)))",
		"__forceinline=inline",
		"_aligned_malloc(x,a)=malloc(x)",
		"_aligned_free(x)=free(x)",
		"stricmp=strcasecmp",
		"SSE_VERSION=3",
		"MAC_COCOA=1",
		"COCOA=1"
    }
    
    links 
	{
	}

   defines { "MAC=1", "PPC=0", "WINDOWS=0",  }
   
   buildoptions { "-std=c++14", "-stdlib=libc++" }
   buildoptions { "-mmacosx-version-min=10.9" }
   linkoptions { "-mmacosx-version-min=10.9" }

   platforms { "x64" }

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
		"_CRT_SECURE_NO_WARNINGS" 
	}

	nuget { "libpng-msvc-x64:1.6.33.8807" }
   
   characterset "MBCS"
   buildoptions { "/MP" }
   
   includedirs {
      "libs/wtl"
    }
	
   flags { "StaticRuntime", "NoMinimalRebuild" }

   platforms { "x64" }

   configuration {}
end

includedirs {
    "libs/xml",
    "src/common/vt_dsp",
    "src/common/thread",
    "vst3sdk/vstgui4",
	 "libs/"
}

-- PLUGIN COMMON --

function plugincommon()

    targetprefix ""
	targetname "Surge"

	files {
		"src/common/**.cpp",
		"src/common/**.h",
		"bitmaps/**.png",
		"libs/xml/tinyxml.cpp",
		"libs/xml/tinyxmlerror.cpp",
		"libs/xml/tinyxmlparser.cpp",
		"src/common/vt_dsp/*.cpp",
		"src/common/thread/*.cpp",
		"vst3sdk/pluginterfaces/base/*.cpp",
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
			"Cocoa.framework", 
			"CoreFoundation.framework",
			"OpenGL.framework",
			"QuartzCore.framework",
		}

	elseif (os.istarget("windows")) then

		pchheader "precompiled.h"
		pchsource "src/common/precompiled.cpp"
		buildoptions { "/FI precompiled.h", "/wd\"4244\"" }
	
		files
		{
			"src/windows/**.cpp",
			"src/windows/**.h",
			"src/windows/**.rc",
			"resources/bitmaps/*.png",
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


-- VST2 PLUGIN --

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
    "vst3sdk/public.sdk/source/vst2.x/**.cpp",
    VSTGUI .. "plugin-bindings/aeffguieditor.cpp",
    }

excludes {
    VSTGUI .. "plugguieditor.cpp",
}

includedirs {
   "src/vst2",
   "vst3sdk"
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
    postbuildcommands { "./package-vst.sh" }
    
    files
    {
        "libs/vst/*.mm"
    }
	
elseif (os.istarget("windows")) then

    linkoptions { "/DEF:resources\\windows-vst2\\surge.def" }

end

-- VST3 PLUGIN --

project "surge-vst3"
kind "SharedLib"
uuid "A87FBED5-3E7A-432F-8611-B2C61F40F4B8"
targetextension ".vst3"

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
    }

excludes {
    VSTGUI .. "aeffguieditor.cpp",
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

    postbuildcommands { "./package-vst3.sh" }
    
    files
    {
		"vst3sdk/public.sdk/source/main/macmain.cpp",
       "vst3sdk/*.mm"
    }
	
elseif (os.istarget("windows")) then

    linkoptions { "/DEF:resources\\windows-vst3\\surge.def" }
	
	files
	{
		"vst3sdk/public.sdk/source/main/dllmain.cpp",
	}
	
    flags { "NoImportLib" }
    
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
		"TARGET_AU=1",
		"PLUGGUI=1",
	}
	
	links 
	{ 
		"AudioToolbox.framework",
	}

	files 
	{
		"src/au/**.cpp",
		"src/au/**.h",
		"libs/AudioUnits/AUPublic/**.cpp",
		"libs/AudioUnits/AUPublic/**.h",
		"libs/AudioUnits/PublicUtility/*.cpp",
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
	
	postbuildcommands { "./package-au.sh" }
	
end