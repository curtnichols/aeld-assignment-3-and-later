#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e    # Exit immediately if a command exits with a non-zero status.
set -u    # Treat unset variables as an error when substituting.

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
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

    # xTODO: Add your kernel build steps here

    # "deep clean" the kernel build tree - removing the `.config` file with any existing configurations
    make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- mrproper

    # Configure for our "virt" ARM dev board we will simulate in QEMU
    make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- defconfig

    # Build a kernel image for booting with QEMU
    make -j16 ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- all

    # build any kernel modules
    make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- modules

    # build the device tree
    make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- dtbs

    cp ./vmlinux ${OUTDIR}
    cp ./arch/arm64/boot/Image ${OUTDIR}
fi

echo "Adding the Image in outdir"

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# xTODO: Create necessary base directories
mkdir -p ${OUTDIR}/rootfs
pushd ${OUTDIR}/rootfs
mkdir -p bin dev etc home lib lib64 proc sbin sys tmp usr var
mkdir -p usr/bin usr/lib usr/sbin
mkdir -p var/log
popd

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # xTODO:  Configure busybox

else
    cd busybox
fi


# xTODO: Make and install busybox

make distclean
make defconfig
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make CONFIG_PREFIX=${OUTDIR}/rootfs ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install

echo "Library dependencies"
${CROSS_COMPILE}readelf -a ./busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a ./busybox | grep "Shared library"

# xTODO: Add library dependencies to rootfs

SYSROOT=$(aarch64-none-linux-gnu-gcc -print-sysroot)
echo "SYSROOT=${SYSROOT}"

# -rwxr-xr-x 1 curt curt 1.1M Sep 12 10:36 ld-linux-aarch64.so.1
# -rwxr-xr-x 1 curt curt  11M Sep 12 10:36 libc.so.6
# -rwxr-xr-x 1 curt curt 2.2M Sep 12 10:36 libm.so.6
# -rwxr-xr-x 1 curt curt 233K Sep 12 10:36 libresolv.so.2

PROGRAM_INTERPRETER_DEST=${OUTDIR}/rootfs/lib
SHARED_LIBS_DEST=${OUTDIR}/rootfs/lib64

echo PROGRAM_INTERPRETER_DEST=${PROGRAM_INTERPRETER_DEST}
echo SHARED_LIBS_DEST=${SHARED_LIBS_DEST}

cp ${SYSROOT}/lib/ld-linux-aarch64.so.1 ${PROGRAM_INTERPRETER_DEST}
cp ${SYSROOT}/lib64/libm.so.6 ${SHARED_LIBS_DEST}
cp ${SYSROOT}/lib64/libresolv.so.2 ${SHARED_LIBS_DEST}
cp ${SYSROOT}/lib64/libc.so.6 ${SHARED_LIBS_DEST}
ls -lh ${OUTDIR}/rootfs/lib

# xTODO: Make device nodes

sudo mknod -m 666 ${OUTDIR}/rootfs/dev/null c 1 3
sudo mknod -m 600 ${OUTDIR}/rootfs/dev/console c 5 1
ls -l ${OUTDIR}/rootfs/dev/

# xTODO: Clean and build the writer utility
pushd ${FINDER_APP_DIR}
make clean
make CROSS_COMPILE=aarch64-none-linux-gnu-

cp writer ${OUTDIR}/rootfs/home

# xTODO: Copy the finder related scripts and executables to the /home directory

cp finder.sh ${OUTDIR}/rootfs/home
cp finder-test.sh ${OUTDIR}/rootfs/home
cp autorun-qemu.sh ${OUTDIR}/rootfs/home

mkdir -p ${OUTDIR}/rootfs/home/conf
cp conf/username.txt ${OUTDIR}/rootfs/home/conf
cp conf/assignment.txt ${OUTDIR}/rootfs/home/conf

popd

# xTODO: Chown the root directory
# wat? I can't find a reference in lectures or my notes;
# assuming the cpio step is good enough.

# xTODO: Create initramfs.cpio.gz
pushd "$OUTDIR/rootfs"
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio

popd
cd ${OUTDIR}
pwd
gzip -f initramfs.cpio
