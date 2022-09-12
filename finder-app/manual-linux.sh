#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.1.10
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    wget https://github.com/torvalds/linux/commit/e33a814e772cdc36436c8c188d8c42d019fda639.diff

    git apply e33a814e772cdc36436c8c188d8c42d019fda639.diff

    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig

    make -j12 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs
      
fi

echo "Adding the Image in outdir"
cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

mkdir -p ${OUTDIR}/rootfs
cd ${OUTDIR}/rootfs
mkdir bin dev etc home lib lib64 proc sbin sys tmp usr var
mkdir usr/bin usr/lib usr/sbin
mkdir -p var/log

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    make distclean
    make defconfig   
else
    cd busybox

fi

make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- 
make ARCH=arm64 CONFIG_PREFIX=${OUTDIR}/rootfs CROSS_COMPILE=aarch64-none-linux-gnu- install

echo "Library dependencies"
${CROSS_COMPILE}readelf -a busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
cd ${OUTDIR}/rootfs

cp /home/chinmay/Documents/AESD/assignment-1/assignment-2/assignment-2-neohere97/finder-app/finder.sh ${OUTDIR}/rootfs/home
cp /home/chinmay/Documents/AESD/assignment-1/assignment-2/assignment-2-neohere97/finder-app/finder-test.sh ${OUTDIR}/rootfs/home
cp /home/chinmay/Documents/AESD/assignment-1/assignment-2/assignment-2-neohere97/finder-app/autorun-qemu.sh ${OUTDIR}/rootfs/home
cp -r /home/chinmay/Documents/AESD/assignment-1/assignment-2/assignment-2-neohere97/finder-app/conf/ ${OUTDIR}/rootfs/home
cp /home/chinmay/Documents/AESD/assignment-1/assignment-2/assignment-2-neohere97/finder-app/writer.c ${OUTDIR}/rootfs/home


# TODO: Make device nodes
cd ${OUTDIR}/rootfs
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 600 dev/console c 5 1


cd /home/chinmay/Documents/AESD/assignment-1/assignment-2/assignment-2-neohere97/finder-app/
make clean
make CROSS_COMPILE=${CROSS_COMPILE}

cp /home/chinmay/Documents/AESD/assignment-1/assignment-2/assignment-2-neohere97/finder-app/writer ${OUTDIR}/rootfs/home

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
cd ${OUTDIR}/rootfs
cp -a /usr/gcc-arm-10.2-2020.11-x86_64-aarch64-none-linux-gnu/lib/. ${OUTDIR}/rootfs/lib/
cp -a /usr/gcc-arm-10.2-2020.11-x86_64-aarch64-none-linux-gnu/lib64/. ${OUTDIR}/rootfs/lib64/


cd ${OUTDIR}/rootfs
sudo chown -R root:root *

# TODO: Create initramfs.cpio.gz
find . | cpio -H newc -ov --owner root:root > ../initramfs.cpio
cd ..
gzip initramfs.cpio
