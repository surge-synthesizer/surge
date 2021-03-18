#
# This file is used to provide targets which stage the extra content external repo
#

set(SURGE_EXTRA_CONTENT_REPO https://github.com/surge-synthesizer/surge-extra-content.git)
set(SURGE_EXTRA_CONTENT_HASH afc591cc06d9adc3dc8dc515a55c66873fa10296)

find_package(Git)
if( ${Git_FOUND} )
    message(STATUS "Adding download-extra-content target from ${SURGE_EXTRA_CONTENT_REPO}" )
    add_custom_target(download-extra-content)
    add_custom_command(TARGET download-extra-content
            POST_BUILD
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMAND cmake -E make_directory surge-extra-content
            COMMAND ${GIT_EXECUTABLE} clone ${SURGE_EXTRA_CONTENT_REPO} surge-extra-content || echo "Already there"
            )
    add_custom_command(TARGET download-extra-content
            POST_BUILD
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/surge-extra-content
            COMMAND ${GIT_EXECUTABLE} checkout ${SURGE_EXTRA_CONTENT_HASH}
            )

    # I explicitly do NOT make this depend on download since download twice in a row fails
    add_custom_target(stage-extra-content)
    add_custom_command(TARGET stage-extra-content
            POST_BUILD
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMAND cmake -E copy_directory ${CMAKE_BINARY_DIR}/surge-extra-content/Skins/ resources/data/skins
            )
else()
    message(STATUS "Absent git, making extra content commands no-ops")
    add_custom_target(download-extra-content)
    add_custom_command(TARGET download-extra-content
            POST_BUILD
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMAND cmake -E echo Skipping Extra Content)
    add_custom_target(stage-extra-content)
    add_custom_command(TARGET stage-extra-content
            POST_BUILD
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMAND cmake -E echo Skipping Extra Content)
endif()
