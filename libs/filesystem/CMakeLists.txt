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

function(use_platform_fs FS_HEADER FS_NAMESPACE)
  add_library(${PROJECT_NAME} INTERFACE)
  generate_header("${FS_HEADER}" "${FS_NAMESPACE}")
endfunction()

set(SURGE_DEVEL_FORCE_FALLBACK_FS OFF CACHE BOOL "Force fallback std::filesystem impl (development only)")
mark_as_advanced(FORCE SURGE_DEVEL_FORCE_FALLBACK_FS)
if (${SURGE_DEVEL_FORCE_FALLBACK_FS})
  use_fallback_fs()
else()
  include(CheckCXXSymbolExists)
  CHECK_CXX_SYMBOL_EXISTS(std::filesystem::path::preferred_separator "filesystem" CXX_STD_FS)
  if (CXX_STD_FS)
    use_platform_fs("<filesystem>" "std::filesystem")
  else()
    # if that check failed we are on one of the wonky gccs in the 7 and 5 family which need to check this with 11 and link stdc++fs
    if (UNIX AND NOT APPLE)
      set (CMAKE_REQUIRED_LIBRARIES "stdc++fs")
    endif()
    CHECK_CXX_SYMBOL_EXISTS(std::experimental::filesystem::path::preferred_separator "experimental/filesystem" CXX_EXP_STD_FS)
    if (CXX_EXP_STD_FS)
      use_platform_fs("<experimental/filesystem>" "std::experimental::filesystem")
      target_link_libraries(${PROJECT_NAME} INTERFACE stdc++fs)
    else()
      set (CMAKE_REQUIRED_LIBRARIES "")
      CHECK_CXX_SYMBOL_EXISTS(std::experimental::filesystem::path::preferred_separator "filesystem" CXX_EXP_STD_FS_FS_HDR)
      if (CXX_EXP_STD_FS_FS_HDR)
        use_platform_fs("<filesystem>" "std::experimental::filesystem")
      else()
        use_fallback_fs()
      endif()
    endif()
  endif()
endif()

add_library(surge::${PROJECT_NAME} ALIAS ${PROJECT_NAME})
