
if( BUILD_SURGE_WITH_DISTRHO_JUCE )
	set( JUCE_LIB_VERSION main )  # If you mod this, change it in scripts/misc/defensive-submodule.sh also:wq
	set( JUCE_REPO git://github.com/DISTRHO/JUCE )
	set( JUCE_LIB_DIR ${CMAKE_SOURCE_DIR}/libs/distrho-juce-${JUCE_LIB_VERSION}/juce )
else()
	set( JUCE_LIB_VERSION 6.0.7 )  # If you mod this, change it in scripts/misc/defensive-submodule.sh also:wq
	set( JUCE_REPO git://github.com/juce-framework/JUCE )
	set( JUCE_LIB_DIR ${CMAKE_SOURCE_DIR}/libs/juce-${JUCE_LIB_VERSION}/juce )
endif()

find_package(Git)

if( NOT Git_FOUND )
  message( ERROR "Can't do JUCE without GIT right now, sorry" )
endif()

if( NOT EXISTS ${JUCE_LIB_DIR}/README.md )
  message( STATUS "Cloning JUCE from ${JUCE_REPO} at tag ${JUCE_LIB_VERSION} into ${JUCE_LIB_DIR}" )
  execute_process (
    COMMAND ${CMAKE_COMMAND} -E make_directory ${JUCE_LIB_DIR}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
  execute_process(
    COMMAND ${GIT_EXECUTABLE} clone ${JUCE_REPO} ${JUCE_LIB_DIR} --depth=1 -b ${JUCE_LIB_VERSION}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
else()
  message( STATUS "Using the JUCE at ${JUCE_LIB_DIR}" )
endif()

message( STATUS "Configuring JUCE into ${JUCE_BINARY_DIR}" )
