#!/bin/bash
#
# build-linux.sh is the master script we use to control the multi-step build
# processes.

help_message()
{
    cat << EOHELP
Usage: $0 [options] <command>

Commands:

    premake         Run premake only.

    build           Run the builds without cleans.
    install         Install built assets.

    clean           Clean all the builds.
    clean-all       Clean all the builds and remove generated files.

    uninstall       Uninstall Surge.

Options:

    -h, --help              Show help.
    -v, --verbose           Verbose output.
    -p, --project=PROJECT   Select a specific PROJECT. Can be either vst2 or vst3.
    -d, --debug             Use a debug version.
    -l, --local             Install/uninstall built assets under /home instead
                            of /usr
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
    project=$1
    echo
    echo "Cleaning build - $project"
    make clean
}

run_build()
{
    project=$1
    mkdir -p build_logs

    echo
    echo Building surge-${project} with output in build_logs/build_${project}.log

    # Since these are piped we lose status from the tee and get wrong return code so
    set -o pipefail

    if [[ -z "$OPTION_verbose" ]]; then
        make ${OPTION_config} surge-${project} 2>&1 | tee build_logs/build_${project}.log
    else
        make ${OPTION_config} surge-${project} verbose=1 2>&1 | tee build_logs/build_${project}.log
    fi

    build_suc=$?
    set +o pipefail
    if [[ $build_suc = 0 ]]; then
        echo ${GREEN}Build of surge-${project} succeeded${NC}
    else
        echo
        echo ${RED}** Build of ${project} failed**${NC}
        grep -i error build_logs/build_${project}.log
        echo
        echo ${RED}** Exiting failed ${project} build**${NC}
        echo Complete information is in build_logs/build_${project}.log

        exit 2
    fi
}

run_all_builds()
{
    run_premake_if

    if [ ! -z "$OPTION_vst2" ]; then
        run_build "vst2"
    fi

    if [ ! -z "$OPTION_vst3" ]; then
        run_build "vst3"
    fi
}

run_install()
{
    echo "Installing presets"
    rsync -r --delete "resources/data/" $OPTION_data_path

    if [ ! -z "$OPTION_vst2" ]; then
        echo "Installing VST2"
        rsync -r -delete $OPTION_vst2_src_path $OPTION_vst2_dest_path
    fi

    if [ ! -z "$OPTION_vst3" ]; then
        echo "Installing VST3"
        rsync -r --delete $OPTION_vst2_src_path $OPTION_vst3_dest_path
    fi
}

run_clean_builds()
{
    if [ ! -e "Makefile" ]; then
        echo "No surge workspace; no builds to clean"
        return 0
    fi

    if [ ! -z "$OPTION_vst2" ]; then
        run_clean "vst2"
    fi

    if [ ! -z "$OPTION_vst3" ]; then
        run_clean "vst3"
    fi
}

run_clean_all()
{
    run_clean_builds

    echo "Cleaning additional assets"
    rm -rf Makefile surge-vst2.make surge-vst3.make surge-app.make build_logs target obj premake-stamp
}

run_uninstall()
{
    rm -rvf $OPTION_data_path

    if [ ! -z "$OPTION_vst2" ]; then
        rm -vf $OPTION_vst2_dest_path
    fi

    if [ ! -z "$OPTION_vst3" ]; then
        rm -vf $OPTION_vst3_dest_path
    fi
}

prerequisite_check

ARGS=$(getopt -o hvp:dl --long help,verbose,project:,debug,local -n "$0" -- "$@") \
    || exit 1
eval set -- "$ARGS"

while true ; do
    case "$1" in
        -h|--help) OPTION_help=1 ; shift ;;
        -v|--verbose) OPTION_verbose=1 ; shift ;;
        -p|--project)
            case "$2" in
                "") shift 2 ;;
                *) OPTION_project=$2 ; shift 2 ;;
            esac ;;
        -d|--debug) OPTION_debug=1 ; shift ;;
        -l|--local) OPTION_local=1 ; shift ;;
        --) shift ; break ;;
        *) break ;;
    esac
done

if [[ ! -z "$OPTION_help" ]]; then
    help_message
    exit 0
fi

if [ -z "$OPTION_project" ] || [ "$OPTION_project" == "vst2" ]; then
    if [ -e "surge-vst2.make" ]; then
        OPTION_vst2=1
    fi
fi

if [ -z "$OPTION_project" ] || [ "$OPTION_project" == "vst3" ]; then
    OPTION_vst3=1
fi

if [ -z "$OPTION_debug" ]; then
    OPTION_config="config=release_x64"
    OPTION_vst2_src_path="target/vst2/Release/Surge.so"
    OPTION_vst3_src_path="target/vst3/Release/Surge.so"
else
    OPTION_config="config=debug_x64"
    OPTION_vst2_src_path="target/vst2/Debug/Surge-Debug.so"
    OPTION_vst3_src_path="target/vst3/Debug/Surge-Debug.so"
fi

if [[ ! -z "$OPTION_local" ]]; then
    OPTION_vst2_dest_path="$HOME/.vst/"
    OPTION_vst3_dest_path="$HOME/.vst3/"
    OPTION_data_path="$HOME/.local/share/Surge"
else
    OPTION_vst2_dest_path="/usr/lib/vst/"
    OPTION_vst3_dest_path="/usr/lib/vst3/"
    OPTION_data_path="/usr/share/Surge"
fi

case $1 in
    premake)
        run_premake
        ;;
    build)
        run_all_builds
        ;;
    install)
        run_install
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
