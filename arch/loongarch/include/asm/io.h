/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020-2021 Loongson Technology Corporation Limited
 */
#ifndef _ASM_IO_H
#define _ASM_IO_H

#define ARCH_HAS_IOREMAP_WC

#include <linux/compiler.h>
#include <linux/kernel.h>
#include <linux/types.h>

#include <asm/addrspace.h>
#include <asm/bug.h>
#include <asm/byteorder.h>
#include <asm/cpu.h>
#include <asm-generic/iomap.h>
#include <asm/page.h>
#include <asm/pgtable-bits.h>
#include <asm/string.h>

#define IO_SPACE_LIMIT 0xffff

/*
 * On LoongArch I/O ports are memory mapped, so we access them using normal
 * load/store instructions. loongarch_io_port_base is the virtual address to
 * which all ports are being mapped.  For sake of efficiency some code
 * assumes that this is an address that can be loaded with a single lui
 * instruction, so the lower 16 bits must be zero. Should be true on any
 * sane architecture; generic code does not use this assumption.
 */
extern unsigned long loongarch_io_port_base;

static inline void set_io_port_base(unsigned long base)
{
	loongarch_io_port_base = base;
}

/*
 * Provide the necessary definitions for generic iomap. We make use of
 * loongarch_io_port_base for iomap(), but we don't reserve any low addresses
 * for use with I/O ports.
 */

#define HAVE_ARCH_PIO_SIZE
#define PIO_OFFSET	loongarch_io_port_base
#define PIO_MASK	IO_SPACE_LIMIT
#define PIO_RESERVED	0x0UL

/*
 * Change "struct page" to physical address.
 */
#define page_to_phys(page)	((dma_addr_t)page_to_pfn(page) << PAGE_SHIFT)
#ifdef CONFIG_64BIT
extern void __init __iomem *early_ioremap(u64 phys_addr, unsigned long size);
#else
extern void __init __iomem *early_ioremap(u32 phys_addr, unsigned long size);
#endif
extern void __init early_iounmap(void __iomem *addr, unsigned long size);
#define early_memremap early_ioremap
#define early_memunmap early_iounmap

static inline void __iomem *ioremap_prot(phys_addr_t offset, unsigned long size,
					 unsigned long prot_val)
{
	return (void __iomem *)(unsigned long)(IO_BASE + offset);
}

#ifdef CONFIG_DMA_NONCOHERENT

extern void (*_dma_cache_wback_inv)(unsigned long start, unsigned long size);
extern void (*_dma_cache_wback)(unsigned long start, unsigned long size);
extern void (*_dma_cache_inv)(unsigned long start, unsigned long size);

#define dma_cache_wback_inv(start, size)    _dma_cache_wback_inv(start, size)
#define dma_cache_wback(start, size)        _dma_cache_wback(start, size)
#define dma_cache_inv(start, size)      _dma_cache_inv(start, size)
#endif

/*
 * ioremap -   map bus memory into CPU space
 * @offset:    bus address of the memory
 * @size:      size of the resource to map
 *
 * ioremap performs a platform specific sequence of operations to
 * make bus memory CPU accessible via the readb/readw/readl/writeb/
 * writew/writel functions and the other mmio helpers. The returned
 * address is not guaranteed to be usable directly as a virtual
 * address.
 */
#define ioremap(offset, size)					\
	ioremap_prot((offset), (size), _CACHE_SUC)
#define ioremap_uc ioremap

/*
 * ioremap_wc - map bus memory into CPU space
 * @offset:     bus address of the memory
 * @size:       size of the resource to map
 *
 * ioremap_wc performs a platform specific sequence of operations to
 * make bus memory CPU accessible via the readb/readw/readl/writeb/
 * writew/writel functions and the other mmio helpers. The returned
 * address is not guaranteed to be usable directly as a virtual
 * address.
 *
 * This version of ioremap ensures that the memory is marked uncachable
 * but accelerated by means of write-combining feature. It is specifically
 * useful for PCIe prefetchable windows, which may vastly improve a
 * communications performance. If it was determined on boot stage, what
 * CPU CCA doesn't support WUC, the method shall fall-back to the
 * _CACHE_SUC option (see cpu_probe() method).
 */
#define ioremap_wc(offset, size)				\
	ioremap_prot((offset), (size), _CACHE_WUC)

/*
 * ioremap_cache -  map bus memory into CPU space
 * @offset:	    bus address of the memory
 * @size:	    size of the resource to map
 *
 * ioremap_cache performs a platform specific sequence of operations to
 * make bus memory CPU accessible via the readb/readw/readl/writeb/
 * writew/writel functions and the other mmio helpers. The returned
 * address is not guaranteed to be usable directly as a virtual
 * address.
 *
 * This version of ioremap ensures that the memory is marked cachable by
 * the CPU.  Also enables full write-combining.	 Useful for some
 * memory-like regions on I/O busses.
 */
#define ioremap_cache(offset, size)				\
	ioremap_prot((offset), (size), _CACHE_CC)

static inline void iounmap(const volatile void __iomem *addr)
{
}

#define mmiowb() asm volatile ("dbar 0" ::: "memory")

/*
 * String version of I/O memory access operations.
 */
#ifdef CONFIG_64BIT
extern void __memset_io(volatile void __iomem *dst, int c, size_t count);
extern void __memcpy_toio(volatile void __iomem *to, const void *from, size_t count);
extern void __memcpy_fromio(void *to, const volatile void __iomem *from, size_t count);
#define memset_io(c, v, l)     __memset_io((c), (v), (l))
#define memcpy_fromio(a, c, l) __memcpy_fromio((a), (c), (l))
#define memcpy_toio(c, a, l)   __memcpy_toio((c), (a), (l))
#endif
#define PCI_IOBASE ((void __iomem *)PIO_OFFSET)

#include <asm-generic/io.h>

#undef PCI_IOBASE

#endif /* _ASM_IO_H */
