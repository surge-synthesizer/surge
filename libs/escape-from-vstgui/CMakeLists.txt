project(escape-from-vstgui VERSION 0.0.0 LANGUAGES CXX)

message(STATUS "Escape is a possibility...")
add_library(escape-from-vstgui INTERFACE)
target_include_directories(escape-from-vstgui INTERFACE include)
target_compile_definitions(escape-from-vstgui INTERFACE ESCAPE_FROM_VSTGUI=1)