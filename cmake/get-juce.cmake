set( JUCE_LIB_VERSION 6.0.1 )
set( JUCE_REPO https://github.com/juce-framework/JUCE )
set( JUCE_LIB_DIR ${CMAKE_SOURCE_DIR}/libs/juce-${JUCE_LIB_VERSION}/juce )

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
    COMMAND ${GIT_EXECUTABLE} clone ${JUCE_REPO} ${JUCE_LIB_DIR}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
  execute_process(
    COMMAND cd ${JUCE_LIB_DIR} && git checkout ${JUCE_LIB_VERSION}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
else()
  message( STATUS "Using the JUCE at ${JUCE_LIB_DIR}" )
endif()

message( STATUS "Configuring JUCE into ${JUCE_BINARY_DIR}" )
