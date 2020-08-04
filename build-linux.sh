#!/bin/bash
#
# build-linux.sh is the master script we use to control the multi-step build
# processes.

help_message()
{
    cat << EOHELP
Usage: $0 [options] <command>

Commands:

    cmake           Run cmake only.

    build           Run the builds without cleans.
    install         Install built assets.

    clean           Clean all the builds.
    clean-all       Clean all the builds and remove generated files.

    uninstall       Uninstall Surge.

Options:

    -h, --help              Show help.
    -v, --verbose           Verbose output.
    -p, --project=PROJECT   Select a specific PROJECT, which can be either
                            vst2, vst3, lv2 or headless.
    -d, --debug             Use a debug version.
    -l, --local             Install/uninstall built assets under /home instead
                            of /usr
EOHELP
}

RED=`tput setaf 1`
GREEN=`tput setaf 2`
NC=`tput sgr0`
DEF_BUILD_DIR="buildlin"

UNAME_M=`uname -m`
if [[ "$UNAME_M" =~ ^arm ]]; then
   ARM=1
fi
if [[ "$UNAME_M" =~ ^aarch ]]; then
   ARM=1
fi

if [[ ! -z $ARM ]]; then
   CMAKE_EXTRA_ARGS="-DARM_NATIVE=native"
   DEF_BUILD_DIR="buildlin-${UNAME_M}"
   echo "ARM build activated. Using ARM_NATIVE=native settings; building in ${DEF_BUILD_DIR}"
fi


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
}

run_cmake()
{
    cmake . -B${DEF_BUILD_DIR} ${CMAKE_EXTRA_ARGS}
}

run_cmake_if()
{
    if [[ ! -f ${DEF_BUILD_DIR}/CMakeCache.txt ]]; then
        run_cmake
    fi
    if [[ CMakeLists.txt -nt ${DEF_BUILD_DIR}/CMakeCache.txt ]]; then
        run_cmake
    fi
}

run_clean()
{
    if [[ -d ${DEF_BUILD_DIR} ]]; then
	cmake --build ${DEF_BUILD_DIR} --target clean --config Release
    fi
}

run_build()
{
    local flavor=$1
    cmake --build ${DEF_BUILD_DIR} --config Release --target $flavor --parallel 2

    build_suc=$?
    if [[ $build_suc = 0 ]]; then
        echo ${GREEN}Build of ${flavor} succeeded${NC}
    else
        echo
        echo ${RED}** Build of ${flavor} failed**${NC}
     
        exit 2
    fi
}

run_builds()
{
    if [ ! -z "$option_vst2" ]; then
        run_cmake_if
        run_build "Surge-VST2-Packaged"
    fi

    if [ ! -z "$option_vst3" ]; then
        run_cmake_if
        run_build "Surge-VST3-Packaged"
    fi

    if [ ! -z "$option_lv2" ]; then
        run_cmake_if
        run_build "Surge-LV2-Packaged"
    fi

    if [ ! -z "$option_headless" ]; then
        run_cmake_if
        run_build "surge-headless"
    fi
}

run_install()
{
    echo "Installing presets"
    rsync -r --delete "resources/data/" $data_path

    if [ ! -z "$option_vst2" ]; then
        echo "Installing VST2"
        rsync -r -delete $vst2_src_path \
                         $vst2_dest_path/$dest_plugin_name
    fi

    if [ ! -z "$option_vst3" ]; then
        echo "Installing VST3"
        # No dest plugin name here since we are a bundle
        rsync -r --delete $vst3_src_path \
                          $vst3_dest_path
    fi

    if [ ! -z "$option_lv2" ]; then
        echo "Installing LV2"
        # No dest plugin name here since we are a bundle
        rsync -r --delete $lv2_src_path \
                          $lv2_dest_path
    fi

    if [ ! -z "$option_headless" ] && [ -d "$headless_dest_path" ]; then
        echo "Installing Headless"
        rsync -r --delete $headless_src_path \
                          $headless_dest_path/$dest_headless_name
    fi
}

run_clean_builds()
{
    run_clean
}

run_clean_all()
{
    run_clean_builds

    echo "Cleaning additional assets"
    rm -rf ${DEF_BUILD_DIR}
}

run_uninstall()
{
    rm -rvf $data_path

    if [ ! -z "$option_vst2" ]; then
        rm -vf $vst2_dest_path/$dest_plugin_name
    fi

    if [ ! -z "$option_vst3" ]; then
	rm -vf $vst3_dest_path/Surge.vst3/Contents/x86_64-linux/$dest_plugin_name
	rmdir -v $vst3_dest_path/Surge.vst3/Contents/x86_64-linux $vst3_dest_path/Surge.vst3/Contents $vst3_dest_path/Surge.vst3
    fi

    if [ ! -z "$option_lv2" ]; then
        rm -vf $lv2_dest_path/$lv2_bundle_name/$dest_plugin_name
        rm -vf $lv2_dest_path/$lv2_bundle_name/*.ttl
        test -d $lv2_dest_path/$lv2_bundle_name && rmdir $lv2_dest_path/$lv2_bundle_name
    fi

    if [ ! -z "$option_headless" ]; then
	rm -vf $headless_dest_path/$dest_headless_name
    fi
}

prerequisite_check

ARGS=$(getopt -o hvp:dl --long help,verbose,project:,debug,local -n "$0" -- "$@") \
    || exit 1
eval set -- "$ARGS"

while true ; do
    case "$1" in
        -h|--help) option_help=1 ; shift ;;
        -v|--verbose) option_verbose=1 ; shift ;;
        -p|--project)
            case "$2" in
                "") shift 2 ;;
                *) option_project=$2 ; shift 2 ;;
            esac ;;
        -d|--debug) option_debug=1 ; shift ;;
        -l|--local) option_local=1 ; shift ;;
        --) shift ; break ;;
        *) break ;;
    esac
done

if [[ ! -z "$option_help" ]]; then
    help_message
    exit 0
fi

if [ -z "$option_project" ] || [ "$option_project" == "vst2" ]; then
    if [ -e "surge-vst2.make" ] || [ ! -z "$VST2SDK_DIR" ]; then
        option_vst2=1
    fi
fi

if [ -z "$option_project" ] || [ "$option_project" == "vst3" ]; then
    option_vst3=1
fi

if [ -z "$option_project" ] || [ "$option_project" == "lv2" ]; then
    option_lv2=1
fi

if [ -z "$option_project" ] || [ "$option_project" == "headless" ]; then
    option_headless=1
fi

if [ -z "$option_debug" ]; then
    config="config=release_x64"
    vst2_src_path="${DEF_BUILD_DIR}/surge_products/Surge.so"
    vst3_src_path="${DEF_BUILD_DIR}/surge_products/Surge.vst3"
    lv2_bundle_name="Surge.lv2"
    lv2_src_path="${DEF_BUILD_DIR}/surge_products/$lv2_bundle_name"
    headless_src_path="${DEF_BUILD_DIR}/surge-headless"
    dest_plugin_name="Surge.so"
    dest_headless_name="Surge-Headless"
else
    config="config=debug_x64"
    vst2_src_path="target/vst2/Debug/Surge-Debug.so"
    vst3_src_path="target/vst3/Debug/Surge-Debug.so"
    lv2_bundle_name="Surge.lv2"
    lv2_src_path="target/lv2/Debug/$lv2_bundle_name"
    headless_src_path="target/headless/Debug/Surge-Debug"
    dest_plugin_name="Surge-Debug.so"
    dest_headless_name="Surge-Headless-Debug"
fi

if [[ ! -z "$option_local" ]]; then
    vst2_dest_path="$HOME/.vst"
    vst3_dest_path="$HOME/.vst3"
    lv2_dest_path="$HOME/.lv2"
    headless_dest_path="$HOME/bin"
    data_path="$HOME/.local/share/surge"
else
    vst2_dest_path="/usr/lib/vst"
    vst3_dest_path="/usr/lib/vst3"
    lv2_dest_path="/usr/lib/lv2"
    headless_dest_path="/usr/bin"
    data_path="/usr/share/surge"
fi

case $1 in
    cmake)
        run_cmake
        ;;
    build)
        run_builds
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
