project(filesystem VERSION 0.0.0 LANGUAGES CXX)

function(generate_header FS_HEADER FS_NAMESPACE)
  message(STATUS "${PROJECT_NAME}: Using ${FS_NAMESPACE} from ${FS_HEADER}")
  set(INC_DIR "${CMAKE_CURRENT_BINARY_DIR}/include")
  set(HEADER_DIR "${INC_DIR}/${PROJECT_NAME}")
  set(HEADER_FILE "${HEADER_DIR}/import.h")
  configure_file(src/import.h.in "${HEADER_FILE}" @ONLY)
  target_sources(${PROJECT_NAME} INTERFACE "${HEADER_FILE}")
  target_include_directories(${PROJECT_NAME} INTERFACE "${INC_DIR}")
endfunction()

function(use_fallback_fs)
  add_library(${PROJECT_NAME}
    src/directory_iterator.cpp
    src/filesystem.cpp
    src/path.cpp
    src/recursive_directory_iterator.cpp
    src/util.h
    include/${PROJECT_NAME}/directory_entry.h
    include/${PROJECT_NAME}/directory_iterator.h
    include/${PROJECT_NAME}/filesystem.h
    include/${PROJECT_NAME}/filesystem_error.h
    include/${PROJECT_NAME}/path.h
    include/${PROJECT_NAME}/recursive_directory_iterator.h
  )
  target_include_directories(${PROJECT_NAME} PUBLIC include)

  target_sources(surge-tests INTERFACE src/tests.cpp)
  generate_header("\"filesystem/filesystem.h\"" "Surge::filesystem")
endfunction()

function(use_platform_fs)
  add_library(${PROJECT_NAME} INTERFACE)
  generate_header("<filesystem>" "std::filesystem")
endfunction()

set(SURGE_DEVEL_FORCE_FALLBACK_FS OFF CACHE BOOL "Force fallback std::filesystem impl (development only)")
mark_as_advanced(FORCE SURGE_DEVEL_FORCE_FALLBACK_FS)
if (${SURGE_DEVEL_FORCE_FALLBACK_FS})
  use_fallback_fs()
else()
  include(CheckCXXSourceCompiles)
  check_cxx_source_compiles("
    #include <filesystem>
    namespace fs = std::filesystem;
    int main(int, char **argv) {
      return fs::recursive_directory_iterator{fs::path{*argv}}.depth();
    }" SURGE_FILESYSTEM_STD_FS_FOUND)
  if (SURGE_FILESYSTEM_STD_FS_FOUND)
    use_platform_fs()
  else()
    use_fallback_fs()
  endif()
endif()

add_library(surge::${PROJECT_NAME} ALIAS ${PROJECT_NAME})
