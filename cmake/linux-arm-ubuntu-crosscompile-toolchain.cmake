#
# This Toolchain is set up for a set of defaults which works on ubuntu-20 cross compile
# to headless. Feel free to add a different toolchain config and then jut send us
# a pull request for different circumstances.
#
# If you make a toolchain in addition to the standard toolchain stuff, also
# set LINUX_ON_ARM to True and export LINUX_ON_ARM_COMPILE_OPTIONS
#
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(LINUX_ON_ARM True)
set(FLAGS
  -march=armv7-a
  -marm
  -mfpu=neon
  -Wno-psabi
  -flax-vector-conversions #FIXME - remove this
  )
string(REPLACE ";" " " FLAGS "${FLAGS}")
set(CMAKE_C_FLAGS_INIT "${FLAGS}" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_INIT "${FLAGS}" CACHE STRING "" FORCE)

set(CMAKE_SYSROOT /usr/bin)
set(CMAKE_STAGING_PREFIX /home/devel/stage)

set(tools /usr)
set(CMAKE_C_COMPILER ${tools}/bin/arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER ${tools}/bin/arm-linux-gnueabihf-g++)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
