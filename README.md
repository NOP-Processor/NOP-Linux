# NOP-Linux

The Linux used in NSCSCC 2023 for [NOP-Core](https://github.com/NOP-Processor/NOP-Core). This is adapted from [la32r-Linux](https://gitee.com/loongson-edu/la32r-Linux).

## Build

- Export `CROSS_COMPILE` environment variable to the prefix of the toolchain, e.g.

    ```bash
    export CROSS_COMPILE=/path/to/la32r-toolchains/install/bin/loongarch32r-linux-gnusf-
    ```
- Configure initramfs path in `arch/loongarch/configs/la32_defconfig`

- Build `vmlinux.strip` for boot
    ```bash
    ./build.sh
    ```