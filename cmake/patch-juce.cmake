find_package(
  PATCH_EXECUTABLE "patch"
  # Add something like below if no patch executable is found
  # HINTS "C:\\Program Files\\Git\\usr\\bin"
  REQUIRED
)

# Look for and apply patches to the third-party (and sub) repo.
file(GLOB PATCHES "${CMAKE_SOURCE_DIR}/src/patches/juce/*.patch")

if (PATCHES)
  foreach(PATCH ${PATCHES})
    message(STATUS "Applying ${PATCH}")
    execute_process(
      # The --binary flag here is a bit of a hack. It covers up problem with mixed line-endings CLRF/LF
      # See: https://stackoverflow.com/a/10934047
      # Really, gitattributes should be used to normalize these, and the below should be used to generate the patch files:
      COMMAND ${PATCH_EXECUTABLE} -p1 --forward --ignore-whitespace --binary --input=${PATCH}
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
        COMMAND ${PATCH_EXECUTABLE} -p1 --reverse --binary --dry-run --input=${PATCH}
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