# vi:set sw=2 et:
set(SURGE_PRODUCT_DIR ${CMAKE_BINARY_DIR}/surge_xt_products)
file(MAKE_DIRECTORY ${SURGE_PRODUCT_DIR})

add_custom_target(surge-xt-distribution)
add_custom_target(surge-staged-assets)

function(surge_juce_package target product_name)
  get_target_property(output_dir ${target} RUNTIME_OUTPUT_DIRECTORY)
  set(pkg_target ${target}_Packaged)
  add_custom_target(${pkg_target} ALL)
  foreach(format ${SURGE_JUCE_FORMATS})
    add_dependencies(${pkg_target} ${target}_${format})
  endforeach()
  if(TARGET ${target}_CLAP)
    add_dependencies(${pkg_target} ${target}_CLAP)
  endif()
  foreach(format AU LV2 Standalone VST VST3 CLAP)
    if(NOT SURGE_COPY_TO_PRODUCTS)
      # Add the copy rule to the pkg_target
      if(TARGET ${target}_${format})
        add_custom_command(
          TARGET ${pkg_target}
          POST_BUILD
          WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
          COMMAND echo "${target}: Relocating ${format} component"
          COMMAND ${CMAKE_COMMAND} -E copy_directory ${output_dir}/${format} ${SURGE_PRODUCT_DIR}/
        )
      endif()
    else()
      # Add the copy rule to the target itself
      if(TARGET ${target}_${format})
        add_custom_command(
          TARGET ${target}_${format}
          POST_BUILD
          WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
          COMMAND echo "${target}: Relocating ${format} component"
          COMMAND ${CMAKE_COMMAND} -E copy_directory ${output_dir}/${format} ${SURGE_PRODUCT_DIR}/
        )
      endif()
    endif()
  endforeach()

  if(TARGET ${target}-cli)
    message(STATUS "Adding ${target}-cli to ${pkg_target}")
    add_dependencies(${pkg_target} ${target}-cli)

    # Add the copy rule to the pkg_target in all cases
    get_target_property(cli_dir ${target}-cli RUNTIME_OUTPUT_DIRECTORY)

    if (WIN32)
      set(dotexe ".exe")
    else()
      set(dotexe "")
    endif()

    add_custom_command(
            TARGET ${pkg_target}
            POST_BUILD
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMAND echo "${target}: Relocating ${target}-cli executable"
            COMMAND ${CMAKE_COMMAND} -E copy ${cli_dir}/${target}-cli${dotexe} ${SURGE_PRODUCT_DIR}/
    )

  endif()

  add_dependencies(surge-staged-assets ${pkg_target})
  add_dependencies(surge-xt-distribution ${pkg_target})

  # Add CMake install rules for UNIX only right now (macOS and Windows use installers)
  if(UNIX AND NOT APPLE)
    if(TARGET ${target}_LV2)
      install(DIRECTORY "${output_dir}/LV2/${product_name}.lv2" DESTINATION ${CMAKE_INSTALL_LIBDIR}/lv2)
    endif()
    if(TARGET ${target}_Standalone)
      install(TARGETS ${target}_Standalone DESTINATION bin)
    endif()
    if(TARGET surge-xt-cli)
      install(TARGETS surge-xt-cli DESTINATION bin)
    endif()
    if(TARGET ${target}_VST3)
      install(DIRECTORY "${output_dir}/VST3/${product_name}.vst3" DESTINATION ${CMAKE_INSTALL_LIBDIR}/vst3)
    endif()
    if(TARGET ${target}_CLAP)
      install(PROGRAMS "${output_dir}/CLAP/${product_name}.clap" DESTINATION ${CMAKE_INSTALL_LIBDIR}/clap)
    endif()
    install(DIRECTORY "${CMAKE_SOURCE_DIR}/resources/data/" DESTINATION share/surge-xt)
  endif()
endfunction()

function(surge_add_lib_subdirectory libname)
  add_subdirectory(${SURGE_SOURCE_DIR}/libs/${libname} ${CMAKE_BINARY_DIR}/libs/${libname} EXCLUDE_FROM_ALL)
endfunction()

function(surge_make_installers)
  set(SURGE_XT_DIST_OUTPUT_DIR ${CMAKE_BINARY_DIR}/surge-xt-dist)
  file(MAKE_DIRECTORY ${SURGE_XT_DIST_OUTPUT_DIR})

  if (DEFINED SURGE_VERSION)
    set (SXTVER ${SURGE_VERSION})
  else()
    set(SXTVER $ENV{SURGE_VERSION})
  endif()

  if("${SXTVER}" STREQUAL "")
    set(SXTVER "LOCAL")
  endif()
  message(STATUS "Installer Surge Version is ${SXTVER}")

  function(run_installer_script PATH)
    add_custom_command(TARGET surge-xt-distribution
      POST_BUILD
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      COMMAND ${CMAKE_SOURCE_DIR}/scripts/${PATH} "${SURGE_PRODUCT_DIR}" "${CMAKE_SOURCE_DIR}" "${SURGE_XT_DIST_OUTPUT_DIR}" "${SXTVER}"
      )
  endfunction()

  if(APPLE)
    run_installer_script(installer_mac/make_installer.sh)
    add_custom_command(TARGET surge-xt-distribution
      POST_BUILD
      USES_TERMINAL
      WORKING_DIRECTORY ${SURGE_PRODUCT_DIR}
      COMMAND zip -r ${SURGE_XT_DIST_OUTPUT_DIR}/surge-xt-macos-${SXTVER}-pluginsonly.zip .
      )
  elseif(UNIX)
    run_installer_script(installer_linux/make_deb.sh)
    run_installer_script(installer_linux/make_rpm.sh)
    add_custom_command(TARGET surge-xt-distribution
      POST_BUILD
      WORKING_DIRECTORY ${SURGE_PRODUCT_DIR}
      COMMAND tar cvzf ${SURGE_XT_DIST_OUTPUT_DIR}/surge-xt-linux-${SXTVER}-pluginsonly.tar.gz .
      )

    set(SURGE_PORTABLE_DIR ${CMAKE_BINARY_DIR}/surge-xt-portable)
    set(portsst "${SURGE_PORTABLE_DIR}/Surge Synth Team")

    add_custom_command(TARGET surge-xt-distribution
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory ${SURGE_PORTABLE_DIR}
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${portsst}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${portsst}"

            COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/resources/surge-shared/README_Portable.txt" "${SURGE_PORTABLE_DIR}/README.txt"

            COMMAND ${CMAKE_COMMAND} -E make_directory "${portsst}/SurgeXTData"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/resources/data" "${portsst}/SurgeXTData"

            COMMAND cd "${SURGE_PORTABLE_DIR}" && tar cvzf ${SURGE_XT_DIST_OUTPUT_DIR}/surge-xt-portable-content-${SXTVER}.tar.gz .
    )

  elseif(WIN32)
    if (${SURGE_BITNESS} EQUAL 64)
      set(SURGE_PORTABLE_DIR ${CMAKE_BINARY_DIR}/surge-xt-portable)
      set(portsst "${SURGE_PORTABLE_DIR}/Surge Synth Team")
      if ("${CMAKE_GENERATOR_PLATFORM}" STREQUAL "arm64ec")
        set(WINARCH "-arm64ec-beta")
      endif()
      add_custom_command(TARGET surge-xt-distribution
        POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory ${SURGE_PORTABLE_DIR}
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${portsst}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${portsst}"

            COMMAND ${CMAKE_COMMAND} -E copy "${SURGE_PRODUCT_DIR}/Surge XT.clap" "${portsst}"
            COMMAND ${CMAKE_COMMAND} -E copy "${SURGE_PRODUCT_DIR}/Surge XT Effects.clap" "${portsst}"
            COMMAND ${CMAKE_COMMAND} -E copy "${SURGE_PRODUCT_DIR}/Surge XT.exe" "${portsst}"
            COMMAND ${CMAKE_COMMAND} -E copy "${SURGE_PRODUCT_DIR}/Surge XT Effects.exe" "${portsst}"
            COMMAND ${CMAKE_COMMAND} -E copy "${SURGE_PRODUCT_DIR}/surge-xt-cli.exe" "${portsst}"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${SURGE_PRODUCT_DIR}/Surge XT.vst3" "${portsst}/Surge XT.vst3"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${SURGE_PRODUCT_DIR}/Surge XT Effects.vst3" "${portsst}/Surge XT Effects.vst3"

            COMMAND 7z a -r ${SURGE_XT_DIST_OUTPUT_DIR}/surge-xt-win${SURGE_BITNESS}${WINARCH}-${SXTVER}-pluginsonly.zip "${portsst}/*"

            COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/resources/surge-shared/README_Portable.txt" "${SURGE_PORTABLE_DIR}/README.txt"

            COMMAND ${CMAKE_COMMAND} -E make_directory "${portsst}/SurgeXTData"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/resources/data" "${portsst}/SurgeXTData"

            COMMAND 7z a -r ${SURGE_XT_DIST_OUTPUT_DIR}/surge-xt-win${SURGE_BITNESS}${WINARCH}-${SXTVER}-portable-install.zip "${SURGE_PORTABLE_DIR}/*"
            )
    else()
      add_custom_command(TARGET surge-xt-distribution
        POST_BUILD
        COMMAND 7z a -r ${SURGE_XT_DIST_OUTPUT_DIR}/surge-xt-win${SURGE_BITNESS}-${SXTVER}-pluginsonly.zip ${SURGE_PRODUCT_DIR}
      )
    endif()
    if ("${CMAKE_GENERATOR_PLATFORM}" STREQUAL "arm64ec")
      message(STATUS "Not making installer for arm64ec")
    else()
      find_program(SURGE_NUGET_EXE nuget.exe PATHS ENV "PATH")
      if(SURGE_NUGET_EXE)
        message(STATUS "Using NUGET from ${SURGE_NUGET_EXE}")
        add_custom_command(TARGET surge-xt-distribution
          POST_BUILD
          WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
          COMMAND ${SURGE_NUGET_EXE} install Tools.InnoSetup -version 6.1.2
          COMMAND Tools.InnoSetup.6.1.2/tools/iscc.exe /O"${SURGE_XT_DIST_OUTPUT_DIR}" /DSURGE_SRC="${CMAKE_SOURCE_DIR}" /DSURGE_BIN="${CMAKE_BINARY_DIR}" "${CMAKE_SOURCE_DIR}/scripts/installer_win/surge${SURGE_BITNESS}.iss"
          )
      else()
        message(STATUS "NuGet not found, not creating InnoSetup installer")
      endif()
    endif()
  endif()
endfunction()
