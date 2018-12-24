#!/bin/sh
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
        --premake                Run premake only

        --build                  Run the builds without cleans
        --install-local          Once assets are built, install them locally
        --build-and-install      Build and install the assets
        --build-validate-au      Build and install the audio unit then validate it

        --clean                  Clean all the builds
        --clean-all              Clean all the builds and remove the xcode files and target directories

        --uninstall-surge        Uninstall surge both from user and system areas. Be very careful!

The default behaviour without arguments is to clean and rebuild everything.

Options are:
        --verbose                Verbose output

Environment variables are:

   VST2SDK_DIR=path      If this points at a valid VST2 SDK, VST2 assets will be built
   BREWBUILD=TRUE        Uses LLVM clang rather than xcode. If you are XCode < 9.4 you will need this

EOHELP
}

RED=`tput setaf 1`
GREEN=`tput setaf 2`
NC=`tput init`

prerequisite_check()
{
    if [ ! -f vst3sdk/LICENSE.txt ] || [ ! -f spdlog/LICENSE ]; then
        echo
        echo ${RED}ERROR: You have not gotten the submodules required to build Surge. Run the following command to get them.${NC}
        echo
        echo git submodule update --init --recursive
        echo
        exit 1
    fi

    # TODO: Check premake is installed
    if [ ! $(which premake5) ]; then
        echo
        echo ${RED}ERROR: You do not have premake5 on your path${NC}
        echo
        echo Please download and install premake from https://premake.github.io per the Surge README.md
        echo
        exit 1
    fi
}

run_premake()
{
    premake5 xcode4
    touch Surge.xcworkspace/premake-stamp
}

run_premake_if()
{
    if [[ premake5.lua -nt Surge.xcworkspace/premake-stamp ]]; then
        run_premake
    fi
}

run_clean()
{
    flavor=$1
    xcodebuild clean -configuration Release -project surge-${flavor}.xcodeproj
}

run_build()
{
    flavor=$1
    mkdir -p build_logs

    echo
    echo Building surge-${flavor} with output in build_logs/build_${flavor}.log
    if [[ -z "$OPTION_verbose" ]]; then
    	xcodebuild build -configuration Release -project surge-${flavor}.xcodeproj > build_logs/build_${flavor}.log
    else
    	xcodebuild build -configuration Release -project surge-${flavor}.xcodeproj | tee build_logs/build_${flavor}.log
    fi

    build_suc=$?
    if [[ $build_suc = 0 ]]; then
        echo ${GREEN}Build of surge-${flavor} succeeded${NC}
    else
        echo
        echo ${RED}** Build of ${flavor} failed**${NC}
        grep -i error build_logs/build_${flavor}.log
        echo
        echo Complete information is in build_logs/build_${flavor}.log

        exit 2
    fi
}

default_action()
{
    run_premake
    if [ -d "surge-vst2.xcodeproj" ]; then
        run_clean "vst2"
        run_build "vst2"
    fi

    run_clean "vst3"
    run_build "vst3"

    run_clean "au"
    run_build "au"
}

run_all_builds()
{
    run_premake_if
    if [ -d "surge-vst2.xcodeproj" ]; then
        run_build "vst2"
    fi

    run_build "vst3"
    run_build "au"
}

run_install_local()
{
    rsync -r "resources/data/" "$HOME/Library/Application Support/Surge/"

    if [ -d "surge-vst2.xcodeproj" ]; then
        rsync -r --delete "products/Surge.vst/" ~/Library/Audio/Plug-Ins/VST/Surge.vst/
    fi
    
    rsync -r --delete "products/Surge.component/" ~/Library/Audio/Plug-Ins/Components/Surge.component/
    rsync -r --delete "products/Surge.vst3/" ~/Library/Audio/Plug-Ins/VST3/Surge.vst3/
}

run_build_validate_au()
{
    run_premake_if
    run_build "au"

    rsync -r "resources/data/" "$HOME/Library/Application Support/Surge/"
    rsync -r --delete "products/Surge.component/" ~/Library/Audio/Plug-Ins/Components/Surge.component/

    auval -vt aumu VmbA
}

run_clean_builds()
{
    run_premake_if
    if [ -d "surge-vst2.xcodeproj" ]; then
        run_clean "vst2"
    fi

    run_clean "vst3"
    run_clean "au"
}

run_clean_all()
{
    run_clean_builds
    rm -rf Surge.xcworkspace *xcodeproj target products build_logs obj
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
    --premake)
        run_premake
        ;;
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
    --clean)
        run_clean_builds
        ;;
    --clean-all)
        run_clean_all
        ;;
    --uninstall-surge)
        run_uninstall_surge
        ;;
    "")
        default_action
        ;;
    *)
        help_message
        ;;
esac

