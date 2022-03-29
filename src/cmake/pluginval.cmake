  message(STATUS "Validate me! Please!")

  if(APPLE)
    set(pluginval_url "https://github.com/Tracktion/pluginval/releases/latest/download/pluginval_macOS.zip")
    set(pluginval_exe pluginval/pluginval.app/Contents/MacOS/pluginval)
  elseif(WIN32)
    set(pluginval_url "https://github.com/Tracktion/pluginval/releases/latest/download/pluginval_Windows.zip")
    set(pluginval_exe pluginval/pluginval.exe)
  else()
    set(pluginval_url "https://github.com/Tracktion/pluginval/releases/latest/download/pluginval_Linux.zip")
    set(pluginval_exe pluginval/pluginval)
  endif()

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

  get_target_property(fxn surge-fx JUCE_PRODUCT_NAME)
  get_target_property(xtn surge-xt JUCE_PRODUCT_NAME)

  if(TARGET surge-xt_VST3)
    create_pluginval_target(surge-xt-pluginval-vst3 surge-xt_VST3 "${xtn}.vst3")
  endif()
  if(TARGET surge-fx_VST3)
    create_pluginval_target(surge-fx-pluginval-vst3 surge-fx_VST3 "${fxn}.vst3")
  endif()

  if(APPLE)
    create_pluginval_target(surge-xt-pluginval-au surge-xt_AU "${xtn}.component")
    create_pluginval_target(surge-fx-pluginval-au surge-fx_AU "${fxn}.component")
  endif()
