project(filesystem VERSION 0.0.0 LANGUAGES CXX)

set(INC_DIR "${CMAKE_CURRENT_BINARY_DIR}/include")
set(HEADER_DIR "${INC_DIR}/${PROJECT_NAME}")

function(generate_header FS_HEADER FS_NAMESPACE)
  message(STATUS "${PROJECT_NAME}: Using ${FS_NAMESPACE} from ${FS_HEADER}")
  set(HEADER_FILE "${HEADER_DIR}/import.h")
  configure_file(src/import.h.in "${HEADER_FILE}" @ONLY)
  target_sources(${PROJECT_NAME} INTERFACE "${HEADER_FILE}")
endfunction()

function(use_ghc_fs)
  # Don't export GHC symbols from ELF shared objects
  set(HEADER_FILE "${HEADER_DIR}/ghc-filesystem.h")
  file(STRINGS ghc-filesystem/include/ghc/filesystem.hpp HEADER_DATA NEWLINE_CONSUME NO_HEX_CONVERSION)
  string(REPLACE "__attribute__((visibility(\"default\")))" "" HEADER_DATA "${HEADER_DATA}")
  file(WRITE "${HEADER_FILE}" ${HEADER_DATA})

  add_library(${PROJECT_NAME} src/ghc-filesystem.cpp "${HEADER_FILE}")
  generate_header("\"filesystem/ghc-filesystem.h\"" "ghc::filesystem")
  target_compile_definitions(${PROJECT_NAME}
    INTERFACE GHC_FILESYSTEM_FWD
    PUBLIC GHC_RAISE_UNICODE_ERRORS GHC_WIN_WSTRING_STRING_TYPE
    PRIVATE GHC_FILESYSTEM_IMPLEMENTATION
  )
  target_include_directories(${PROJECT_NAME} PUBLIC "${INC_DIR}")
endfunction()

function(use_platform_fs)
  add_library(${PROJECT_NAME} INTERFACE)
  generate_header("<filesystem>" "std::filesystem")
  target_include_directories(${PROJECT_NAME} INTERFACE "${INC_DIR}")
endfunction()

set(SURGE_FILESYSTEM_FORCE_GHC OFF CACHE BOOL "Force GHC filesystem implementation")
if (${SURGE_FILESYSTEM_FORCE_GHC})
  use_ghc_fs()
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
    use_ghc_fs()
  endif()
endif()

add_library(surge::${PROJECT_NAME} ALIAS ${PROJECT_NAME})
