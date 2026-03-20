# hi, other surge devs. is this necessary?
project(surge-rs)

if(CMAKE_BUILD_TYPE MATCHES Release)
	set(PROFILE "release")
else()
	set(PROFILE "debug")
endif()

set(SRS_PATH "${CMAKE_CURRENT_SOURCE_DIR}/surge-rs")
set(CARGO_TARGET_DIR "${CMAKE_BINARY_DIR}/src/surge-rs/target")
set(SRS_OUTPUT "${CARGO_TARGET_DIR}/${PROFILE}/libsurge_rs.rlib")

# if you run "make -jX", then this won't trigger.
# ...and you'll use your whole cpu. (probably what you want anyway.)
if(DEFINED CMAKE_BUILD_PARALLEL_LEVEL)
	set(JARGS "--jobs ${CMAKE_BUILD_PARALLEL_LEVEL}")
endif()

file(GLOB_RECURSE SRS_SRC CONFIGURE_DEPENDS
	"${SRS_PATH}/Cargo*"
	"${SRS_PATH}/cpp/*"
	"${SRS_PATH}/src/*"
	"${SRS_PATH}/wrapper.h"
)

# in order to make the output bearable, we filter out anything undesirable.
set(HELPFUL1 ${CMAKE_COMMAND} -E env CARGO_TARGET_DIR=${CARGO_TARGET_DIR})
set(HELPFUL2 $<$<STREQUAL:${PROFILE},release>:--release>)
set(HELPFUL3 $<$<BOOL:${SURGE_BUILD_RS_EXAMPLES}>:--examples>)
add_custom_command(
	OUTPUT ${SRS_OUTPUT}
	COMMAND ${HELPFUL1} cargo build ${HELPFUL2} ${HELPFUL3} ${JARGS}
		--features in-surge-tree --color always -vv
		2>&1 | python3 -u ${CMAKE_CURRENT_SOURCE_DIR}/filter.py
	WORKING_DIRECTORY ${SRS_PATH}
	COMMENT "Building surge-rs (${PROFILE})."
)

file(GLOB_RECURSE DIRS RELATIVE ${CMAKE_BINARY_DIR}/src LIST_DIRECTORIES ON ${CMAKE_BINARY_DIR}/src/*)
foreach(DIR ${DIRS})
	set(DIR2 ${CMAKE_BINARY_DIR}/src/${DIR})
	if(IS_DIRECTORY ${DIR2} AND EXISTS ${DIR2}/CMakeFiles)
		get_property(THIS_TARGET DIRECTORY ${DIR2} PROPERTY BUILDSYSTEM_TARGETS)
		if(THIS_TARGET)
			list(APPEND TARGET_LIST ${THIS_TARGET})
			message(STATUS "[SRS] registered for wait: \"${THIS_TARGET}\".")
		endif()
	endif()
endforeach()

add_custom_target(surge-rs ALL
	DEPENDS ${SRS_OUTPUT} ${TARGET_LIST}
)
