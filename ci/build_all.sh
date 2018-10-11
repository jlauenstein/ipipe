#!/bin/bash
#
# Copyright (c) Siemens AG, 2014-2018

set -x

build_xeno()
{
    pushd ci/xenomai
    ./scripts/bootstrap
    popd
    mkdir xenobuild
    pushd xenobuild
    # use all supplied extra args for configure
    ../ci/xenomai/configure --with-core=cobalt --enable-smp  "$@"
    cat config.log
    make -s -j `nproc` all
    popd
    ls -la . xenobuild
}


if [ "$TARGET" == "i386" ]; then

    sudo apt-get install -qq gcc-multilib

    echo "===== Ipipe/i386 build ====="

    cp ci/conf.i386.ipipe .config
    git status -v
    ls -la .config include/ ci/*; 
    time make -j `nproc` bzImage modules
    ls -l .config vmlinux
    make -s clean

    echo "===== Cobalt/i386 build ====="

    cp ci/conf.i386.xeno .config
    ci/xenomai/scripts/prepare-kernel.sh --arch=x86 --verbose
    time make -j `nproc` bzImage modules
    ls -l .config vmlinux

    build_xeno --enable-pshared --host=i686-linux "CFLAGS=-m32 -O2" "LDFLAGS=-m32"
#   make -s clean

elif [ "$TARGET" == "arm" ]; then

    sudo apt-get install -qq gcc-arm-linux-gnueabihf
    sudo apt-get install -y pkg-config-arm-linux-gnueabihf

    echo "===== Ipipe/arm build ====="

    cp ci/conf.arm.ipipe .config
    grep CONFIG_IPIPE .config
#    time make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- bzImage modules
#    ls -l .config vmlinux

    ci/xenomai/scripts/prepare-kernel.sh --arch=arm --verbose

    build_xeno --build=i686-pc-linux-gnu --host=arm-linux-gnueabihf "CFLAGS=-march=armv7-a -mfpu=vfp3" "LDFLAGS=-march=armv7-a -mfpu=vfp3"

#   build_xeno --build=i686-pc-linux-gnu --host=arm-linux-gnueabihf- "CFLAGS=-march=armv7-a -mfpu=vfp3" "LDFLAGS=-march=armv7-a -mfpu=vfp3"
#   make -s clean


elif [ "$TARGET" == "native" ]; then

    echo "===== NoIpipe/x86 build ====="

    cp ci/conf.x86.noipipe .config
    grep CONFIG_IPIPE .config
    make -j `nproc` bzImage modules
    ls -l .config vmlinux
    make clean

    echo "===== Ipipe/x86 build ====="
    cp ci/conf.x86.ipipe .config
    grep CONFIG_IPIPE .config
    make -j `nproc` bzImage modules
    ls -l .config vmlinux
    make clean

    echo "===== Cobalt/x86 build ====="
    cp ci/conf.x86.xeno .config
    ci/xenomai/scripts/prepare-kernel.sh --arch=x86 --verbose
    git status -v
    ls -la .config include/ ci/*; 
    make -j `nproc` bzImage modules

    build_xeno "--enable-pshared"

else
    echo "===== No TARGET set ====="
    exit 1
fi
