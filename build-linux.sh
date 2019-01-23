#!/bin/bash
#
# build-linux.sh is the master script we use to control the multi-step build
# processes.
#
# To understand what it does, run
#
# ./build-linux.sh --help

help_message()
{
    cat << EOHELP

build-linux.sh is the master script that we use to control the multi-step
build processes.

To see this message run

   ./build-linux.sh --help

To do a simple local build which is not installed do

   ./build-linux.sh

Other usages take the form of

   ./build-linux.sh <command> <options?>

Commands are:

        --help                   Show this message
        --premake                Run premake only

        --build                  Run the builds without cleans
        --install                Once assets are built, install them locally
        --build-and-install      Build and install the assets

        --clean                  Clean all the builds
        --clean-all              Clean all the builds and remove generated files

        --uninstall              Uninstall Surge

The default behaviour without arguments is to clean and rebuild everything.

Options are:
        --verbose                Verbose output

Environment variables are:

   VST2SDK_DIR=path             If this points at a valid VST2 SDK, VST2 assets
                                will be built

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
    premake5 --cc=gcc --os=linux gmake2
    touch premake-stamp
}

run_premake_if()
{
    if [[ premake5.lua -nt premake-stamp ]]; then
        run_premake
    fi
}

run_clean()
{
    flavor=$1
    echo
    echo "Cleaning build - $flavor"
    make clean
}

run_build()
{
    flavor=$1
    mkdir -p build_logs

    echo
    echo Building surge-${flavor} with output in build_logs/build_${flavor}.log
    # Since these are piped we lose status from the tee and get wrong return code so
    set -o pipefail
    if [[ -z "$OPTION_verbose" ]]; then
        make config=release_x64 2>&1 | tee build_logs/build_${flavor}.log
    else
    	make config=release_x64 verbose=1 2>&1 | tee build_logs/build_${flavor}.log
    fi

    build_suc=$?
    set +o pipefail
    if [[ $build_suc = 0 ]]; then
        echo ${GREEN}Build of surge-${flavor} succeeded${NC}
    else
        echo
        echo ${RED}** Build of ${flavor} failed**${NC}
        grep -i error build_logs/build_${flavor}.log
        echo
        echo ${RED}** Exiting failed ${flavor} build**${NC}
        echo Complete information is in build_logs/build_${flavor}.log

        exit 2
    fi
}

default_action()
{
    run_premake
    if [ -e "surge-vst2.make" ]; then
        run_clean "vst2"
        run_build "vst2"
    fi

    run_clean "vst3"
    run_build "vst3"
}

run_all_builds()
{
    run_premake_if
    if [ -e "surge-vst2.make" ]; then
        run_build "vst2"
    fi

    run_build "vst3"
}

run_install_local()
{
    echo "Installing configuration.xml"
    rsync -r "resources/data/configuration.xml" "$HOME/.local/share/Surge/"

    if [ -e "surge-vst2.make" ]; then
        echo "Installing VST2"
        rsync -r -delete "target/vst2/Release/Surge.so" $HOME/.vst/
    fi

    echo "Installing VST3"
    rsync -r --delete "target/vst3/Release/Surge.so" $HOME/.vst3/
}

run_clean_builds()
{
    if [ ! -e "Makefile" ]; then
        echo "No surge workspace; no builds to clean"
        return 0
    fi

    if [ -e "surge-vst2.make" ]; then
        run_clean "vst2"
    fi

    run_clean "vst3"
}

run_clean_all()
{
    run_clean_builds

    echo "Cleaning additional assets"
    rm -rf Makefile surge-vst2.make surge-vst3.make surge-app.make build_logs
}

run_uninstall()
{
    rm -vf ~/.local/share/Surge/configuration.xml
    rm -vf ~/.vst/Surge.so
    rm -vf ~/.vst3/Surge.so
}

command="$1"

for option in ${@:2}
do
	eval OPTION_${option//-}="true"
done

# Put help up here so it runs even without the prerequisite check.
if [ "$command" = "--help" ]; then
    help_message
    exit 0
fi

prerequisite_check

case $command in
    --premake)
        run_premake
        ;;
    --build)
        run_all_builds
        ;;
    --install)
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
    --uninstall)
        run_uninstall
        ;;
    "")
        default_action
        ;;
    *)
        help_message
        ;;
esac
