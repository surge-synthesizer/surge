
find_package(Git)

if( DEFINED GIT_EXECUTABLE )
  execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
    WORKING_DIRECTORY ${SURGESRC}
    OUTPUT_VARIABLE GIT_BRANCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    )

  execute_process(
    COMMAND ${GIT_EXECUTABLE} log -1 --format=%h
    WORKING_DIRECTORY ${SURGESRC}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    )
else()
  message( WARNING "Git isn't present in your path. Setting hashes to defaults" )
  set( GIT_BRANCH "git-not-present" )
  set( GIT_COMMIT_HASH "git-not-present" )
endif()

cmake_host_system_information(RESULT SURGE_BUILD_FQDN QUERY FQDN )

message( STATUS "Setting up surge version" )
message( STATUS "  git hash is ${GIT_COMMIT_HASH} and branch is ${GIT_BRANCH}" )
message( STATUS "  buildhost is ${SURGE_BUILD_FQDN}" )

if( ${AZURE_PIPELINE} )
  message( STATUS "Azure Pipeline Build" )
  set( lpipeline "pipeline" )
else()
  message( STATUS "Developer Local Build" )
  set( lpipeline "local" )
endif()

if(${GIT_BRANCH} STREQUAL "master" )
  if( ${AZURE_PIPELINES} )
    set( lverpatch "nightly" )
  else()
    set( lverpatch "master" )
  endif()
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
set( SURGE_BUILD_LOCATION "${lpipeline}" )

message( STATUS "Using SURGE_VERSION=${SURGE_FULL_VERSION}" )

message( STATUS "Configuring ${SURGEBLD}/geninclude/version.cpp" )
configure_file( ${SURGESRC}/src/common/version.cpp.in
  ${SURGEBLD}/geninclude/version.cpp )

if( WIN32 )
  message( STATUS "Configuring surgeversion.rc" )
  configure_file( ${SURGESRC}/src/windows/surgeversion.rc.in
    ${SURGEBLD}/geninclude/surgeversion.rc
    )
endif()
  
