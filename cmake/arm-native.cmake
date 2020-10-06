set( LINUX_ON_ARM True )
set(FLAGS
  -march=native
  -mcpu=native
  -Wno-psabi
  -flax-vector-conversions #FIXME - remove this
  )
string(REPLACE ";" " " FLAGS "${FLAGS}")
set(CMAKE_C_FLAGS_INIT "${FLAGS}" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_INIT "${FLAGS}" CACHE STRING "" FORCE)
