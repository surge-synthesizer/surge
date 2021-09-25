# vi:set sw=2 et:

#
# Usage: cmake -P lua2cpp.cmake <out-dir> <out-cpp> <out-hdr> <namespace> <source.lua...>
#
set(out_dir ${CMAKE_ARGV3})
set(out_cpp ${CMAKE_ARGV4})
set(out_hdr ${CMAKE_ARGV5})
set(namespace "namespace ${CMAKE_ARGV6}")

foreach(arg RANGE 7 ${CMAKE_ARGC})
  set(file "${CMAKE_ARGV${arg}}")
  if(EXISTS "${file}")
    string(REGEX REPLACE "\\.lua$" "" var "${file}")
    list(APPEND vars "${var}")
    file(READ "${file}" "${var}_value")
  endif()
endforeach()

# Generate source
set(src "#include \"${out_hdr}\"\n\n${namespace} {\n\n")
foreach(var ${vars})
  set(src "${src}const std::string ${var} = R\"EOF(${${var}_value}\n)EOF\";\n\n")
endforeach()
set(src "${src}} // ${namespace}\n")
file(WRITE "${out_dir}/${out_cpp}" "${src}")

# Generate header
string(REPLACE ";" ", " src "${vars}")
set(src "#pragma once\n\n#include <string>\n\n${namespace} {\n\nextern const std::string ${src};\n\n} // ${namespace}\n")
file(WRITE "${out_dir}/include/${out_hdr}" "${src}")
