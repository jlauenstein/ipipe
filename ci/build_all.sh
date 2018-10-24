#!/bin/bash
#
# Copyright (c) Siemens AG, 2014-2018

set -x
# set -e

KRNL_CFGS="noipipe ipipe"

build_kernel()
{
    cp ci/conf.${TARGET}.${KRNL_CFG} .config
    make olddefconfig

    make ARCH=$TARGET CROSS_COMPILE=$CROSS_COMPILE -j `nproc` bzImage modules
    
    ls -l .config vmlinux
    make -s ARCH=$TARGET CROSS_COMPILE=$CROSS_COMPILE clean
}

build_xeno()
{
    pushd ci/xenomai
    ./scripts/bootstrap
    popd
    
    mkdir xenobuild
    pushd xenobuild
    # use all supplied extra args for configure
    ../ci/xenomai/configure --with-core=cobalt --enable-smp "$@"

    make -s -j `nproc` all
    popd
}

################################################################

case $TARGET in
    x86)
        CROSS_COMPILE=
        XENO_ARCH=x86
        XENO_OPTS="--enable-pshared"
        ;;
    i386)
        sudo apt-get install -qq --no-install-recommends gcc-multilib

        CROSS_COMPILE=
        XENO_ARCH=x86
        XENO_OPTS="--enable-pshared --host=i686-linux 'CFLAGS=-m32 -O2' \"LDFLAGS=-m32\""
        ;;
    arm)
        sudo apt-get install -qq gcc-arm-linux-gnueabihf >/dev/null

        CROSS_COMPILE=arm-linux-gnueabihf-
        XENO_ARCH=arm
        XENO_OPTS='--build=i686-pc-linux-gnu --host=arm-linux-gnueabihf CC=arm-linux-gnueabihf-gcc "CFLAGS=-march=armv7-a -mfpu=vfp3" "LDFLAGS=-march=armv7-a -mfpu=vfp3"'
        ;;
    *)
        echo "===== Error TARGET: $TARGET ====="
        exit 1
        ;;
esac

for KRNL_CFG in $KRNL_CFGS; do

    echo "===== $TARGET/$KRNL_CFG build ====="
    build_kernel

done

KRNL_CFG=xeno
echo "===== $TARGET/$KRNL_CFG build ====="

ci/xenomai/scripts/prepare-kernel.sh --arch=$XENO_ARCH --verbose

build_kernel

build_xeno $XENO_OPTS

exit 0

####################################################################
if [ "$TARGET" == "i386" ]; then

    sudo apt-get install -qq --no-install-recommends gcc-multilib

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

    cfg_opts="--enable-pshared --host=i686-linux \"CFLAGS=-m32\ -O2\" \"LDFLAGS=-m32\""
    
    build_xeno $cfg_opts
#   build_xeno --enable-pshared --host=i686-linux "CFLAGS=-m32 -O2" "LDFLAGS=-m32"
#   make -s clean

elif [ "$TARGET" == "arm" ]; then

    sudo apt-get install -qq gcc-arm-linux-gnueabihf >/dev/null

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

elif [ "$TARGET" == "x86" ]; then

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
