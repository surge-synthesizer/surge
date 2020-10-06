set( LINUX_ON_ARM True )
set(LINUX_ON_ARM_COMPILE_OPTIONS
  -march=native
  -mcpu=native
  -Wno-psabi
  -flax-vector-conversions #FIXME - remove this
  )

