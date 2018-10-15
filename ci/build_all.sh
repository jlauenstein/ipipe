#!/bin/bash
#
# Copyright (c) Siemens AG, 2014-2018

set -x
# set -e

build_xeno()
{
    pushd ci/xenomai
    ./scripts/bootstrap
    popd
    
    mkdir xenobuild
    pushd xenobuild
    
    # use all supplied extra args for configure
    ../ci/xenomai/configure --with-core=cobalt --enable-smp  "$@"

    make -s -j `nproc` all
    popd
}


if [ "$TARGET" == "i386" ]; then

    sudo apt-get install -qq gcc-multilib

    echo "===== NoIpipe/i386 build ====="

    cp ci/conf.i386.noipipe .config
    make ARCH=i386 -j `nproc` bzImage modules
    ls -l .config vmlinux
    make -s clean

    echo "===== Ipipe/i386 build ====="

    cp ci/conf.i386.ipipe .config
    make ARCH=i386 -j `nproc` bzImage modules
    ls -l .config vmlinux
    make -s clean

    echo "===== Cobalt/i386 build ====="

    cp ci/conf.i386.xeno .config
    ci/xenomai/scripts/prepare-kernel.sh --arch=x86 --verbose
    make ARCH=i386 -j `nproc` bzImage modules
    ls -l .config vmlinux

    build_xeno --enable-pshared --host=i686-linux "CFLAGS=-m32 -O2" "LDFLAGS=-m32"
#   make -s clean

elif [ "$TARGET" == "arm" ]; then

    sudo apt-get install -qq gcc-arm-linux-gnueabihf

   echo "===== NoIpipe/arm build ====="

    cp ci/conf.arm.noipipe .config
    make ARCH=arm -j `nproc` CROSS_COMPILE=arm-linux-gnueabihf- bzImage modules
    ls -l .config vmlinux
    make clean

    echo "===== Ipipe/arm build ====="

    cp ci/conf.arm.ipipe .config
    make ARCH=arm -j `nproc` CROSS_COMPILE=arm-linux-gnueabihf- bzImage modules
    ls -l .config vmlinux
    make clean

    echo "===== Cobalt/arm build ====="

    cp ci/conf.arm.xeno .config
    ci/xenomai/scripts/prepare-kernel.sh --arch=arm --verbose
    make ARCH=arm -j `nproc` CROSS_COMPILE=arm-linux-gnueabihf- bzImage modules
    ls -l .config vmlinux

    build_xeno --build=i686-pc-linux-gnu --host=arm-linux-gnueabihf CC=arm-linux-gnueabihf-gcc "CFLAGS=-march=armv7-a -mfpu=vfp3" "LDFLAGS=-march=armv7-a -mfpu=vfp3"
#   make -s clean

elif [ "$TARGET" == "native" ]; then

    echo "===== NoIpipe/x86 build ====="

    cp ci/conf.x86.noipipe .config
    make -j `nproc` bzImage modules
    ls -l .config vmlinux
    make clean

    echo "===== Ipipe/x86 build ====="
    cp ci/conf.x86.ipipe .config
    make -j `nproc` bzImage modules
    ls -l .config vmlinux
    make clean

    echo "===== Cobalt/x86 build ====="
    cp ci/conf.x86.xeno .config
    ci/xenomai/scripts/prepare-kernel.sh --arch=x86 --verbose
    make -j `nproc` bzImage modules

    build_xeno "--enable-pshared"

else
    echo "===== No TARGET set ====="
    exit 1
fi
