#!/bin/bash
#
# Copyright (c) Siemens AG, 2014-2018

echo "===== Cobalt/i386 build ====="
set -x

cp ci/conf.i386.xeno .config
ci/xenomai/scripts/prepare-kernel.sh --arch=x86 --verbose
git status -v
ls -la .config include/ ci/*; 
time make -j `nproc` bzImage modules
pushd ci/xenomai; ./scripts/bootstrap; popd
mkdir xenobuild
pushd xenobuild
../ci/xenomai/configure --with-core=cobalt --enable-smp --enable-pshared --host=i686-linux CFLAGS="-m32 -O2" LDFLAGS="-m32"
make -s -j `nproc` all
ls -la . xenobuild
make clean
popd
make clean

echo "===== Ipipe/arm build ====="

cp ci/conf.arm.ipipe .config
grep CONFIG_IPIPE .config
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- bzImage modules
ls -l .config vmlinux
make clean

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
pushd ci/xenomai; ./scripts/bootstrap; popd
# mkdir xenobuild
pushd xenobuild
../ci/xenomai/configure --with-core=cobalt --enable-smp --enable-pshared
make -s -j `nproc` all
popd
ls -la . xenobuild
