  message(STATUS "Validate me! Please!")

  if (APPLE)
    set(pluginval_url "https://github.com/Tracktion/pluginval/releases/latest/download/pluginval_macOS.zip")
    set(pluginval_exe pluginval/pluginval.app/Contents/MacOS/pluginval)
  elseif (WIN32)
    set(pluginval_url "https://github.com/Tracktion/pluginval/releases/latest/download/pluginval_Windows.zip")
    set(pluginval_exe pluginval/pluginval.exe)
  else ()
    set(pluginval_url "https://github.com/Tracktion/pluginval/releases/latest/download/pluginval_Linux.zip")
    set(pluginval_exe pluginval/pluginval)
  endif ()

  add_custom_target(stage-pluginval)
  add_custom_command(TARGET stage-pluginval
          POST_BUILD
          WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
          COMMAND cmake -E make_directory pluginval
          COMMAND curl -L ${pluginval_url} -o pluginval/pluginval.zip
          COMMAND cd pluginval && unzip -o pluginval.zip
          )

  add_custom_target(surge-pluginval-all)
  function(create_pluginval_target name target plugin)
    message(STATUS "Creating pluginval target for plugin: ${plugin}")
    add_custom_target(${name})
    add_dependencies(${name} ${target})
    add_dependencies(${name} stage-pluginval)
    get_target_property(plugin_location ${target} LIBRARY_OUTPUT_DIRECTORY)
    add_custom_command(TARGET ${name}
            POST_BUILD
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMAND ${pluginval_exe} --validate-in-process --output-dir "." --validate "${plugin_location}/${plugin_name}" || exit 1
            )
    add_dependencies(surge-pluginval-all ${name})
  endfunction()

  create_pluginval_target(surge-xt-pluginval-vst3 surge-xt_VST3 "Surge XT.vst3")
  create_pluginval_target(surge-fx-pluginval-vst3 surge-fx_VST3 "${SURGE_FX_PRODUCT_NAME}.vst3")

  if (APPLE)
    create_pluginval_target(surge-xt-pluginval-au surge-xt_AU "Surge XT.component")
    create_pluginval_target(surge-fx-pluginval-au surge-fx_AU "${SURGE_FX_PRODUCT_NAME}.component")
  endif ()
