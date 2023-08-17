/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Author: Huacai Chen <chenhuacai@loongson.cn>
 * Copyright (C) 2020-2021 Loongson Technology Corporation Limited
 */

#ifndef __ASM_MACH_LOONGSON32_LOONGSON_H
#define __ASM_MACH_LOONGSON32_LOONGSON_H

#include <linux/init.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/pci.h>
#include <asm/addrspace.h>

#include <boot_param.h>

extern const struct plat_smp_ops loongson3_smp_ops;

/* loongson-specific command line, env and memory initialization */
extern void __init fw_init_environ(void);
extern void __init fw_init_memory(void);
extern void __init fw_init_numa_memory(void);

#define LOONGSON_REG(x) \
	(*(volatile u32 *)((char *)TO_UNCAC(LOONGSON_REG_BASE) + (x)))

#define LOONGSON_LIO_BASE	0x18000000
#define LOONGSON_LIO_SIZE	0x00100000	/* 1M */
#define LOONGSON_LIO_TOP	(LOONGSON_LIO_BASE+LOONGSON_LIO_SIZE-1)

#define LOONGSON_BOOT_BASE	0x1c000000
#define LOONGSON_BOOT_SIZE	0x02000000	/* 32M */
#define LOONGSON_BOOT_TOP	(LOONGSON_BOOT_BASE+LOONGSON_BOOT_SIZE-1)

#define LOONGSON_REG_BASE	0x1fe00000
#define LOONGSON_REG_SIZE	0x00100000	/* 1M */
#define LOONGSON_REG_TOP	(LOONGSON_REG_BASE+LOONGSON_REG_SIZE-1)

/* GPIO Regs - r/w */

#define LOONGSON_GPIODATA		LOONGSON_REG(0x11c)
#define LOONGSON_GPIOIE			LOONGSON_REG(0x120)
#define LOONGSON_REG_GPIO_BASE          (LOONGSON_REG_BASE + 0x11c)

#define MAX_PACKAGES 4

/* Chip Config registor of each physical cpu package */
extern u32 loongson_chipcfg[MAX_PACKAGES];
#define LOONGSON_CHIPCFG(id) (*(volatile u32 *)(loongson_chipcfg[id]))

/* Chip Temperature registor of each physical cpu package */
extern u32 loongson_chiptemp[MAX_PACKAGES];
#define LOONGSON_CHIPTEMP(id) (*(volatile u32 *)(loongson_chiptemp[id]))

/* Freq Control register of each physical cpu package */
extern u32 loongson_freqctrl[MAX_PACKAGES];
#define LOONGSON_FREQCTRL(id) (*(volatile u32 *)(loongson_freqctrl[id]))

#define xconf_readl(addr) readl(addr)
#define xconf_readq(addr) readq(addr)

static inline void xconf_writel(u32 val, volatile void __iomem *addr)
{
	asm volatile (
	"	st.w	%[v], %[hw], 0	\n"
	"	ld.b	$r0, %[hw], 0	\n"
	:
	: [hw] "r" (addr), [v] "r" (val)
	);
}

static inline void xconf_writeq(u64 val64, volatile void __iomem *addr)
{
	asm volatile (
	"	st.d	%[v], %[hw], 0	\n"
	"	ld.b	$r0, %[hw], 0	\n"
	:
	: [hw] "r" (addr),  [v] "r" (val64)
	);
}

#endif /* __ASM_MACH_LOONGSON32_LOONGSON_H */
