# Cross Compiling for ARM on Ubuntu 20

1. Install the toolchain

sudo apt-get install libc6-armel-cross libc6-dev-armel-cross binutils-arm-linux-gnueabi libncurses5-dev build-essential bison flex libssl-dev bc

sudo apt-get install gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf


2. Run cmake

make -Bbuild-arm -DCMAKE_TOOLCHAIN_FILE=cmake/linux-arm-toolchain.cmake
