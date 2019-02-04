#!/bin/bash
#
# build-linux.sh is the master script we use to control the multi-step build
# processes.

help_message()
{
    cat << EOHELP
Usage: $0 [options] <command>

Commands:

    premake         Run premake only

    build           Run the builds without cleans
    install-local   Once assets are built, install them locally

    clean           Clean all the builds
    clean-all       Clean all the builds and remove generated files

    uninstall       Uninstall Surge

Options:

    -h, --help      Show help
    -v, --verbose   Verbose output
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
    echo "Installing local configuration.xml"
    rsync -r "resources/data/configuration.xml" "$HOME/.local/share/Surge/"

    echo "Installing local presets"
    rsync -r --delete "resources/data/" "$HOME/.local/share/Surge"

    if [ -e "surge-vst2.make" ]; then
        echo "Installing local VST2"
        rsync -r -delete "target/vst2/Release/Surge.so" $HOME/.vst/
    fi

    echo "Installing local VST3"
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
    rm -rf Makefile surge-vst2.make surge-vst3.make surge-app.make build_logs target obj premake-stamp
}

run_uninstall()
{
    rm -vf ~/.local/share/Surge/configuration.xml
    rm -vf ~/.vst/Surge.so
    rm -vf ~/.vst3/Surge.so
}

prerequisite_check

ARGS=$(getopt -o hv --long help,verbose -n "$0" -- "$@")
eval set -- "$ARGS"

while true ; do
    case "$1" in
        -h|--help) OPTION_help=1 ; shift ;;
        -v|--verbose) OPTION_verbose=1 ; shift ;;
        --) shift ; break ;;
        *) break ;;
    esac
done

if [[ ! -z "$OPTION_help" ]]; then
    help_message
    exit 0
fi

case $1 in
    premake)
        run_premake
        ;;
    build)
        run_all_builds
        ;;
    install-local)
        run_install_local
        ;;
    clean)
        run_clean_builds
        ;;
    clean-all)
        run_clean_all
        ;;
    uninstall)
        run_uninstall
        ;;
    *)
        echo $0: missing operand
        echo Try \'$0 --help\' for more information.
        ;;
esac
