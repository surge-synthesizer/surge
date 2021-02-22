project(oddsound-mts VERSION 0.0.0 LANGUAGES CXX)

add_library(${PROJECT_NAME}
      MTS-ESP/Client/libMTSClient.cpp
)

target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_NAME} MTS-ESP/Client/)
add_library(surge::${PROJECT_NAME} ALIAS ${PROJECT_NAME})
