# vi:set sw=2 et:
project(surgepy)

add_subdirectory(${CMAKE_SOURCE_DIR}/libs/pybind11 pybind11)
pybind11_add_module(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE surgepy.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE
  surge::surge-common
  )

if(UNIX AND NOT APPLE)
  find_package(Threads REQUIRED)
  target_link_libraries(${PROJECT_NAME}
    PRIVATE
    Threads::Threads
    ${PYTHON_LIBRARIES}
    )

  if(CMAKE_SYSTEM_NAME MATCHES "BSD")
    target_link_libraries(${PROJECT_NAME} PRIVATE execinfo)
  endif()
endif()
