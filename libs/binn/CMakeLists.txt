project(binn)

add_library(binn binn.c include/binn/binn.h)
target_include_directories(binn INTERFACE include)
target_include_directories(binn PRIVATE include/binn)
add_library(surge::binn ALIAS binn)
