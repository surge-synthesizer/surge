
find_package(Git)
execute_process(
  COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_BRANCH
  OUTPUT_STRIP_TRAILING_WHITESPACE
  )

execute_process(
  COMMAND ${GIT_EXECUTABLE} log -1 --format=%h
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_COMMIT_HASH
  OUTPUT_STRIP_TRAILING_WHITESPACE
  )

cmake_host_system_information(RESULT BUILD_FQDN QUERY FQDN )

message( STATUS "Setting up surge version" )
message( STATUS "  git hash is ${GIT_COMMIT_HASH} and branch is ${GIT_BRANCH}" )
message( STATUS "  buildhost is ${BUILD_FQDN}" )


if(${GIT_BRANCH} STREQUAL "master" )
  set( lverpatch "nightly" )
  set( lverrel "999" )
else()
  set( lverpatch ${GIT_BRANCH} )
  set( lverrel "1000" )
endif()

set( SURGE_FULL_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${lverpatch}.${GIT_COMMIT_HASH}" )
set( SURGE_MAJOR_VERSION "${PROJECT_VERSION_MAJOR}" )
set( SURGE_SUB_VERSION "${PROJECT_VERSION_MINOR}" )
set( SURGE_RELEASE_VERSION "${lverpatch}" )
set( SURGE_RELEASE_NUMBER "${lverrel}" )
set( SURGE_BUILD_HASH "${GIT_COMMIT_HASH}" )

message( STATUS "Using SURGE_VERSION=${SURGE_FULL_VERSION}" )

configure_file( ${CMAKE_SOURCE_DIR}/src/common/version.h.in
  ${CMAKE_BINARY_DIR}/geninclude/version.h )

if( WIN32 )
  message( STATUS "Configuring surgeversion.rc" )
  configure_file( ${CMAKE_SOURCE_DIR}/src/windows/surgeversion.rc.in
    ${CMAKE_BINARY_DIR}/geninclude/surgeversion.rc
    )
endif()
  
