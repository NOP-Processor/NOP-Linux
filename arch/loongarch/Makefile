# SPDX-License-Identifier: GPL-2.0
#
# Author: Huacai Chen <chenhuacai@loongson.cn>
# Copyright (C) 2020-2021 Loongson Technology Corporation Limited
#
KBUILD_DEFCONFIG := loongson3_defconfig
#ifdef CONFIG_32BIT
KBUILD_DTBS      := dtbs
#endif
#
# Select the object file format to substitute into the linker script.
#
32bit-tool-archpref     = loongarch32r
64bit-tool-archpref	= loongarch64
32bit-bfd		= elf32-loongarch
64bit-bfd		= elf64-loongarch
32bit-emul		= elf32loongarch
64bit-emul		= elf64loongarch
ifdef CONFIG_32BIT
tool-archpref           =$(32bit-tool-archpref)
UTS_MACHINE             := loongarch32r
endif

ifdef CONFIG_64BIT
tool-archpref		= $(64bit-tool-archpref)
UTS_MACHINE		:= loongarch64
endif

ifneq ($(SUBARCH),$(ARCH))
  ifeq ($(CROSS_COMPILE),)
    CROSS_COMPILE := $(call cc-cross-prefix, $(tool-archpref)-linux-  $(tool-archpref)-linux-gnu-  $(tool-archpref)-unknown-linux-gnu-)
  endif
endif
cflags-y += $(call cc-option, -mno-check-zero-division)
ifdef CONFIG_32BIT
ld-emul                 = $(32bit-emul)
vmlinux-32              = vmlinux.32

cflags-y                += -mabi=ilp32s
endif


ifdef CONFIG_64BIT
ld-emul			= $(64bit-emul)
cflags-y		+= -mabi=lp64
endif

all-y			:= vmlinux

#
# GCC uses -G 0 -mabicalls -fpic as default.  We don't want PIC in the kernel
# code since it only slows down the whole thing.  At some point we might make
# use of global pointer optimizations but their use of $r2 conflicts with
# the current pointer optimization.
#
cflags-y			+= -G 0 -pipe
cflags-y			+= -msoft-float
LDFLAGS_vmlinux			+= -G 0 -static -n -nostdlib
KBUILD_AFLAGS_KERNEL		+= -Wa,-mla-global-with-pcrel
KBUILD_CFLAGS_KERNEL		+= -Wa,-mla-global-with-pcrel
KBUILD_AFLAGS_MODULE		+= -Wa,-mla-global-with-abs
KBUILD_CFLAGS_MODULE		+= -fno-plt -Wa,-mla-global-with-abs,-mla-local-with-abs
#ifdef CONFIG_32BIT
KBUILD_CFLAGS_MODULE            += -fno-optimize-sibling-calls
ifneq ($(call as-option,-Wa$(comma)-msoft-float,),)
        cflags-y                += -DGAS_HAS_SET_HARDFLOAT -Wa,-msoft-float
endif
toolchain-virt                          := $(call cc-option-yn,$(loongarch-cflags) -mvirt)
cflags-$(toolchain-virt)                += -DTOOLCHAIN_SUPPORTS_VIRT
toolchain-crc                           := $(call cc-option-yn,$(loongarch-cflags) -Wa$(comma)-mcrc)
cflags-$(toolchain-crc)                 += -DTOOLCHAIN_SUPPORTS_CRC
#endif
cflags-y += -ffreestanding
# Some distribution-specific toolchains might pass the -fstack-check
# option during the build, which adds a simple stack-probe at the beginning
# of every function.  This stack probe is to ensure that there is enough
# stack space, else a SEGV is generated.  This is not desirable for LoongArch
# as kernel stacks are small, placed in unmapped virtual memory, and do not
# grow when overflowed.
#
cflags-y += -fno-stack-check

#
# Board-dependent options and extra files
#
include arch/loongarch/Kbuild.platforms

ifdef CONFIG_PHYSICAL_START
load-y				= $(CONFIG_PHYSICAL_START)
endif

drivers-$(CONFIG_PCI)		+= arch/loongarch/pci/

KBUILD_AFLAGS	+= $(cflags-y)
KBUILD_CFLAGS	+= $(cflags-y)
KBUILD_CPPFLAGS += -DVMLINUX_LOAD_ADDRESS=$(load-y)
bootvars-y	= VMLINUX_LOAD_ADDRESS=$(load-y) PLATFORM="$(platform-y)"
ifdef CONFIG_32BIT
bootvars-y      += ADDR_BITS=32
endif

ifdef CONFIG_64BIT
bootvars-y	+= ADDR_BITS=64
endif

# This is required to get dwarf unwinding tables into .debug_frame
# instead of .eh_frame so we don't discard them.
KBUILD_CFLAGS += -fno-asynchronous-unwind-tables

KBUILD_LDFLAGS	+= -m $(ld-emul)

ifdef CONFIG_LOONGARCH
CHECKFLAGS += $(shell $(CC) $(KBUILD_CFLAGS) -dM -E -x c /dev/null | \
	egrep -vw '__GNUC_(|MINOR_|PATCHLEVEL_)_' | \
	sed -e "s/^\#define /-D'/" -e "s/ /'='/" -e "s/$$/'/" -e 's/\$$/&&/g')
endif

head-y := arch/loongarch/kernel/head.o

libs-y += arch/loongarch/lib/

prepare: vdso_prepare
vdso_prepare: prepare0
	$(Q)$(MAKE) $(build)=arch/loongarch/vdso include/generated/vdso-offsets.h

PHONY += vdso_install
vdso_install:
	$(Q)$(MAKE) $(build)=arch/loongarch/vdso $@

# boot image targets (arch/loongarch/boot/)
boot-y := vmlinux.bin
all:	$(all-y) $(KBUILD_DTBS)

# boot
$(boot-y): vmlinux FORCE
	$(Q)$(MAKE) $(build)=arch/loongarch/boot VMLINUX=vmlinux \
		$(bootvars-y) arch/loongarch/boot/$@
CLEAN_FILES += vmlinux
# device-trees
core-$(CONFIG_BUILTIN_DTB) += arch/loongarch/boot/dts/

install:
	$(Q)install -D -m 755 vmlinux $(INSTALL_PATH)/vmlinux-$(KERNELRELEASE)
	$(Q)install -D -m 644 .config $(INSTALL_PATH)/config-$(KERNELRELEASE)
	$(Q)install -D -m 644 System.map $(INSTALL_PATH)/System.map-$(KERNELRELEASE)

define archhelp
	echo '  install              - install kernel into $(INSTALL_PATH)'
	echo '  vmlinux.bin          - Raw binary boot image'
	echo
endef
