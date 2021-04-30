set(FOUND_PATCH_EXECUTABLE 0)

# Try to use FindPatch() and then find_program() to find a "patch" binary, fatal error if none found
# [CMake +3.10] https://cmake.org/cmake/help/latest/module/FindPatch.html
find_package(Patch)
message("Searching for patch using FindPatch()")
if(Patch_FOUND)
  message("Patch found: ${Patch_EXECUTABLE}")
  set(FOUND_PATCH_EXECUTABLE ${Patch_EXECUTABLE})
else()
  message("Searching for patch using find_program()")
  find_program(
    PATCH_EXECUTABLE
    NAMES "patch"
    # This is for my personal use
    HINTS "C:\\Program Files\\Git\\usr\\bin"
  )
  if(PATCH_EXECUTABLE)
    set(FOUND_PATCH_EXECUTABLE ${PATCH_EXECUTABLE})
  else()
    message(FATAL_ERROR "Unable to find a patch executable with either of FindPatch() or find_program()")
  endif()
endif()

# Sanity check
if (NOT FOUND_PATCH_EXECUTABLE)
  message(FATAL_ERROR "Failed both patch executable searches but continued execution. Stopping, as this was unintended.")
endif()
message("FOUND_PATCH_EXECUTABLE=${FOUND_PATCH_EXECUTABLE}")

################################################################

# Look for and apply patches to the third-party (and sub) repo.
file(GLOB PATCHES "${CMAKE_SOURCE_DIR}/src/patches/juce/*.patch")

if (PATCHES)
  foreach(PATCH ${PATCHES})
    message(STATUS "Applying ${PATCH}")
    execute_process(
      # The --binary flag here is a bit of a hack. It covers up problem with mixed line-endings CLRF/LF
      # See: https://stackoverflow.com/a/10934047
      # Really, gitattributes should be used to normalize these, and the below should be used to generate the patch files:
      COMMAND ${FOUND_PATCH_EXECUTABLE} -p1 --forward --ignore-whitespace --binary --input=${PATCH}
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/libs/juce"
      OUTPUT_VARIABLE OUTPUT
      RESULT_VARIABLE RESULT
      COMMAND_ECHO STDOUT)
    if (RESULT EQUAL 0)
      message(STATUS "Patch applied: ${PATCH}")
    else()
      # Unfortunately although patch will recognise that a patch is already
      # applied it will still return an error.
      execute_process(
        COMMAND ${FOUND_PATCH_EXECUTABLE} -p1 --reverse --binary --dry-run --input=${PATCH}
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/libs/juce"
        OUTPUT_VARIABLE OUTPUT
        RESULT_VARIABLE RESULT2
        COMMAND_ECHO STDOUT)
      if (RESULT2 EQUAL 0)
        message(STATUS "Patch was already applied: ${PATCH}")
      else()
        message(FATAL_ERROR "Error applying patch ${PATCH}")
      endif()
    endif()
  endforeach(PATCH)
endif()