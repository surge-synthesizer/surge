project(oddsound-mts VERSION 0.0.0 LANGUAGES CXX)

add_library(${PROJECT_NAME}
      MTS-ESP/Client/libMTSClient.cpp
      MTS-ESP/Master/libMTSMaster.cpp
)

target_include_directories(${PROJECT_NAME} PUBLIC MTS-ESP/Client MTS-ESP/Master)
target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_DL_LIBS})
add_library(surge::${PROJECT_NAME} ALIAS ${PROJECT_NAME})
