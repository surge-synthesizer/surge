# vi:set sw=2 et:
function(surge_install_resources scope dst)
  add_custom_target(install-resources-${scope})
  add_custom_command(
    TARGET install-resources-${scope}
    POST_BUILD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND echo "Installing resources to ${dst}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${dst}"
    COMMAND rsync -r --delete "data/" "${dst}"
  )
endfunction()

if(APPLE)
  surge_install_resources(local "$ENV{HOME}/Library/Application Support/Surge XT/")
  surge_install_resources(global "/Library/Application Support/Surge XT/")
elseif(UNIX)
  surge_install_resources(local "$ENV{HOME}/.local/share/surge-xt/")
  surge_install_resources(global "${CMAKE_INSTALL_PREFIX}/share/surge-xt/")
endif()
