#!/bin/bash
#
# build-osx.sh is the master script we use to control the multi-step build processes
#
# To understand what it does, run
#
# ./build-osx.sh --help


help_message()
{
    cat << EOHELP

build-osx.sh is the master script we use to control the multi-step build processes
You can do a lot with xcode but for things like starting releasing and installing
locally you will need to interact with this script

To see this message run

   ./build-osx.sh --help

To do a simple local build which is not installed do

   ./build-osx.sh

Other usages take the form of

   ./build-osx.sh <command> <options?>

Commands are:

        --help                   Show this message

        --build                  Run the builds without cleans
        --install-local          Once assets are built, install them locally
        --build-and-install      Build and install the assets
        --build-validate-au      Build and install the audio unit then validate it
        --build-install-vst2     Build and install only the VST2
        --build-install-vst3     Build and install only the VST3
        --build-headless         Build the headless application

        --get-and-build-fx       Get and build the surge-fx project. This is only needed if you
                                 want to make a release with that asset included

        --package                Creates a .pkg file from current built state in products
        --clean-and-package      Cleans everything; runs all the builds; makes an installer; drops it in products
                                 Equivalent of running --clean-all then --build then --package

        --clean                  Clean all the builds
        --clean-all              Clean all the builds and remove the xcode files and target directories

        --uninstall-surge        Uninstall surge both from user and system areas. Be very careful!

We also have a few useful defaults to run in a DAW if you have the DAW installed. Your favorite dev combo
not listed here? Feel free to add one and send in a Pull Request!

        --run-hosting-au         Run the Audio Unit in Hosting AU
        --run-logic-au           Run the Audio Unit in Logic Pro X
        --run-reaper-vst3        Run the VST3 in Reaper64

The default behaviour without arguments is to clean and rebuild everything.

Increasingly, build-osx.sh is just a convenience wrapper on running cmake + xcode. If you want
you can do almost all the core things here from the xcode project cmake ejects.

Options are:
        --verbose                Verbose output

Environment variables are:

   VST2SDK_DIR=path             If this points at a valid VST2 SDK, VST2 assets will be built
   BREWBUILD=TRUE               Uses LLVM clang rather than xcode. If you are XCode < 9.4 you will need this
   SURGE_USE_VECTOR_SKIN={skin} Uses the new vector skins in assets/classic-vector in built asset.

      For SURGE_USE_VECTOR_SKIN you need to give the name of a subdirectory in assets. For instance

            SURGE_USE_VECTOR_SKIN=original-vector ./build-osx.sh --build-validate-au

      will use the 'original-vector' skin and locally build an AU

EOHELP
}

RED=`tput setaf 1`
GREEN=`tput setaf 2`
NC=`tput init`

prerequisite_check()
{

    if [ ! -f vst3sdk/LICENSE.txt ]; then
        echo
        echo ${RED}ERROR: You have not gotten the submodules required to build Surge. Run the following command to get them.${NC}
        echo
        echo git submodule update --init --recursive
        echo
        exit 1
    fi

    if [ ! $(which cmake) ]; then
        echo
        echo ${RED}ERROR: You do not have cmake on your path${NC}
        echo
  	    echo Please install cmake. "brew install cmake" or visit https://cmake.org
        echo
        exit 1
    fi

    if [ ! $(which xcpretty) ]; then
        echo
        echo ${GREEN}You will get better output if you `brew install xcpretty`${NC}
        echo
    fi

}


run_clean()
{
    flavor=$1
    echo
    echo "Cleaning build - $flavor"
    xcodebuild clean -configuration Release -project build/Surge.xcodeproj/ -target $flavor
}

run_cmake_build()
{
    target=$1

    echo Building ${target} with cmake

    # Don't let TEE eat my return status
    mkdir -p build
    cmake -GXcode -Bbuild

    if [ $(which xcpretty) ]; then
        set -o pipefail && xcodebuild build -configuration Release -project build/Surge.xcodeproj -target ${target} | xcpretty
    else
        xcodebuild build -configuration Release -project build/Surge.xcodeproj -target ${target}
    fi



    build_suc=$?

    if [[ $build_suc = 0 ]]; then
        echo ${GREEN}Build of $target succeeded${NC}
    else
        echo
        echo ${RED}** Build of $target failed**${NC}

        exit 2
    fi
}

default_action()
{
    run_cmake_build "all-components"
}

run_all_builds()
{
    default_action
}

run_install_local()
{
    run_cmake_build "install-everything-local"
}

run_build_validate_au()
{
    run_cmake_build "validate-au"
}

run_hosting_au()
{
    if [ ! -d "/Applications/Hosting AU.app" ]; then
        echo
        echo "Hosting AU is not installed. Please install it from http://ju-x.com/hostingau.html"
        echo
        exit 1;
    fi

    "/Applications/Hosting AU.app/Contents/MacOS/Hosting AU" "test-data/daw-files/mac/Surge.hosting"
}

run_logic_au()
{
    if [ ! -d "/Applications/Logic Pro X.app" ]; then
        echo
        echo "Logic is not installed. Please install it!"
        echo
        exit 1;
    fi

    "/Applications/Logic Pro X.app/Contents/MacOS/Logic Pro X" "test-data/daw-files/mac/Surge_AU.logicx"
}

run_reaper_vst3()
{
    if [ ! -d "/Applications/REAPER64.app" ]; then
        echo
        echo "REAPER64 is not installed. Please install it!"
        echo
        exit 1;
    fi

    "/Applications/REAPER64.app/Contents/MacOS/REAPER" "test-data/daw-files/mac/Surge_VST3.RPP"
}

run_build_install_vst2()
{
    run_cmake_build "install-vst2-local"
}

run_build_install_vst3()
{
    run_cmake_build "install-vst3-local"
}

run_clean_builds()
{
    if [ ! -d "build/Surge.xcodeproj" ]; then
        echo "No surge workspace; no builds to clean"
        return 0
    else
        xcodebuild clean -project build/Surge.xcodeproj
    fi
}

run_clean_all()
{
    run_clean_builds

    echo "Cleaning additional assets (directories, XCode, etc)"
    rm -rf build products
}

run_uninstall_surge()
{
    sudo rm -rf /Library/Audio/Plug-Ins/Components/Surge.component
    sudo rm -rf /Library/Audio/Plug-Ins/VST/Surge.vst
    sudo rm -rf /Library/Audio/Plug-Ins/VST3/Surge.vst3
    sudo rm -rf ~/Library/Audio/Plug-Ins/Components/Surge.component
    sudo rm -rf ~/Library/Audio/Plug-Ins/VST/Surge.vst
    sudo rm -rf ~/Library/Audio/Plug-Ins/VST3/Surge.vst3
    sudo rm -rf ~/Library/Application\ Support/Surge
}

run_package()
{
    pkgver=`cat VERSION`beta-`(date +%Y-%m-%d-%H%M)`
    echo "Building with version [${pkgver}]"
    cd installer_mac/
    ./make_installer.sh ${pkgver}
    cd ..

    mv installer_mac/installer/*${pkgver}*.pkg products/

    echo
    echo "Package completed and in products/ directory"
    echo
    ls -l products/*${pkgver}*.pkg
    echo
    echo "Have a lovely day!"
    echo
}

get_and_build_fx()
{
    set -x
    mkdir -p fxbuild
    mkdir -p products
    cd fxbuild
    git clone https://github.com/surge-synthesizer/surge-fx
    cd surge-fx
    git submodule update --init --recursive
    make -f Makefile.mac build
    cd Builds/MacOSX/build/Release
    tar cf - SurgeEffectsBank.component/* | ( cd ../../../../../../products ; tar xf - )
    tar cf - SurgeEffectsBank.vst3/* | ( cd   ../../../../../../products ; tar xf - )
}

# This is the main section of the script
command="$1"

for option in ${@:2}
do
	eval OPTION_${option//-}="true"
done

# put help up here so it runs even without the prerequisite check
if [ "$command" = "--help" ]; then
    help_message
    exit 0
fi

prerequisite_check

# if you add a Key here then you need to implement it as a function and add it to the help above
# to get the PR swept. Thanks!
case $command in
    --build)
        run_all_builds
        ;;
    --install-local)
        run_install_local
        ;;
    --build-and-install)
        run_all_builds
        run_install_local
        ;;
    --build-validate-au)
        run_build_validate_au
        ;;
    --run-hosting-au)
        run_build_validate_au
        run_hosting_au
        ;;
    --run-logic-au)
        run_build_validate_au
        run_logic_au
        ;;
    --run-reaper-vst3)
        run_build_install_vst3
        run_reaper_vst3
        ;;
    --build-install-vst2)
        run_build_install_vst2
        ;;
    --build-install-vst3)
        run_build_install_vst3
        ;;
    --build-headless)
        run_cmake_build "surge-headless"
        ;;
    --run-headless)
        run_cmake_build "run-headless"
        ;;
    --clean)
        run_clean_builds
        ;;
    --clean-all)
        run_clean_all
        ;;
    --clean-and-package)
        run_clean_all
        run_all_builds
        run_package
        ;;
    --package)
        run_package
        ;;
    --uninstall-surge)
        run_uninstall_surge
        ;;
    --get-and-build-fx)
        get_and_build_fx
        ;;
    "")
        default_action
        ;;
    *)
        help_message
        ;;
esac

