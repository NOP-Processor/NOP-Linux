#!/bin/bash

set -ex

if [ -z ${CROSS_COMPILE} ]; then
    echo "CROSS_COMPILE is not set"
    exit 1
fi

export ARCH=loongarch
OUT=la_build
rm -r ${OUT}
mkdir ${OUT}
make la32_defconfig O=${OUT}
make clean
make vmlinux -j  O=${OUT} 2>&1 | tee -a build_error.log
${CROSS_COMPILE}strip ${OUT}/vmlinux -o ${OUT}/vmlinux.strip
${CROSS_COMPILE}objdump -d ${OUT}/vmlinux > ${OUT}/vmlinux.S