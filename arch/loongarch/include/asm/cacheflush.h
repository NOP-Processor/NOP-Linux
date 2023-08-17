/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020-2021 Loongson Technology Corporation Limited
 */
#ifndef _ASM_CACHEFLUSH_H
#define _ASM_CACHEFLUSH_H

#include <linux/mm.h>
#include <asm/cpu-features.h>
#include <asm/cacheops.h>
//#include <asm/unroll.h>
/*
 * This flag is used to indicate that the page pointed to by a pte
 * is dirty and requires cleaning before returning it to the user.
 */
#define PG_dcache_dirty			PG_arch_1

#define Page_dcache_dirty(page)		\
	test_bit(PG_dcache_dirty, &(page)->flags)
#define SetPageDcacheDirty(page)	\
	set_bit(PG_dcache_dirty, &(page)->flags)
#define ClearPageDcacheDirty(page)	\
	clear_bit(PG_dcache_dirty, &(page)->flags)

extern void local_flush_icache_range(unsigned long start, unsigned long end);

#define flush_icache_range	local_flush_icache_range
#define flush_icache_user_range	local_flush_icache_range

extern void copy_to_user_page(struct vm_area_struct *vma,
	struct page *page, unsigned long vaddr, void *dst, const void *src,
	unsigned long len);

extern void copy_from_user_page(struct vm_area_struct *vma,
	struct page *page, unsigned long vaddr, void *dst, const void *src,
	unsigned long len);

#define ARCH_IMPLEMENTS_FLUSH_DCACHE_PAGE 1

#define flush_cache_all()				do { } while (0)
#define flush_cache_mm(mm)				do { } while (0)
#define flush_cache_dup_mm(mm)				do { } while (0)
#define flush_cache_range(vma, start, end)		do { } while (0)
#define flush_cache_page(vma, vmaddr, pfn)		do { } while (0)
#define flush_cache_vmap(start, end)			do { } while (0)
#define flush_cache_vunmap(start, end)			do { } while (0)
#define flush_icache_page(vma, page)			do { } while (0)
#define flush_icache_user_page(vma, page, addr, len)	do { } while (0)
#define flush_dcache_page(page)				do { } while (0)
#define flush_dcache_mmap_lock(mapping)			do { } while (0)
#define flush_dcache_mmap_unlock(mapping)		do { } while (0)

#define cache_op(op, addr)						\
	__asm__ __volatile__(						\
	"	cacop	%0, %1					\n"	\
	:								\
	: "i" (op), "R" (*(unsigned char *)(addr)))

static inline void flush_icache_line_indexed(unsigned long addr)
{
	cache_op(Index_Invalidate_I, addr);
}

static inline void flush_dcache_line_indexed(unsigned long addr)
{
	cache_op(Index_Writeback_Inv_D, addr);
}

static inline void flush_icache_line(unsigned long addr)
{
	cache_op(Hit_Invalidate_I, addr);
}

static inline void flush_dcache_line(unsigned long addr)
{
	cache_op(Hit_Writeback_Inv_D, addr);
}
#ifdef CONFIG_32BIT
#define cache16_unroll32(base,op)                   \
     __asm__ __volatile__(                       \
     "   cacop %1,%0, 0x000; cacop %1, %0,0x010  ;  \n" \
     "   cacop %1,%0, 0x020; cacop %1, %0,0x030  ;  \n" \
     "   cacop %1,%0, 0x040; cacop %1, %0,0x050  ;  \n" \
     "   cacop %1,%0, 0x060; cacop %1, %0,0x070  ;  \n" \
     "   cacop %1,%0, 0x080; cacop %1, %0,0x090  ;  \n" \
     "   cacop %1,%0, 0x0a0; cacop %1, %0,0x0b0  ;  \n" \
     "   cacop %1,%0, 0x0c0; cacop %1, %0,0x0d0  ;  \n" \
     "   cacop %1,%0, 0x0e0; cacop %1, %0,0x0f0  ;  \n" \
     "   cacop %1,%0, 0x100; cacop %1, %0,0x110  ;  \n" \
     "   cacop %1,%0, 0x120; cacop %1, %0,0x130  ;  \n" \
     "   cacop %1,%0, 0x140; cacop %1, %0,0x150  ;  \n" \
     "   cacop %1,%0, 0x160; cacop %1, %0,0x170  ;  \n" \
     "   cacop %1,%0, 0x180; cacop %1, %0,0x190  ;  \n" \
     "   cacop %1,%0, 0x1a0; cacop %1, %0,0x1b0  ;  \n" \
     "   cacop %1,%0, 0x1c0; cacop %1, %0,0x1d0  ;  \n" \
     "   cacop %1,%0, 0x1e0; cacop %1, %0,0x1f0  ;  \n" \
         :                           \
         : "r" (base),                       \
           "i" (op));

#define cache32_unroll32(base,op)                   \
     __asm__ __volatile__(                       \
     "   cacop %1, %0,0x000; cacop %1, %0,0x020  ;  \n" \
     "   cacop %1, %0,0x040; cacop %1, %0,0x060  ;  \n" \
     "   cacop %1, %0,0x080; cacop %1, %0,0x0a0  ;  \n" \
     "   cacop %1, %0,0x0c0; cacop %1, %0,0x0e0  ;  \n" \
     "   cacop %1, %0,0x100; cacop %1, %0,0x120  ;  \n" \
     "   cacop %1, %0,0x140; cacop %1, %0,0x160  ;  \n" \
     "   cacop %1, %0,0x180; cacop %1, %0,0x1a0  ;  \n" \
     "   cacop %1, %0,0x1c0; cacop %1, %0,0x1e0  ;  \n" \
     "   cacop %1, %0,0x200; cacop %1, %0,0x220  ;  \n" \
     "   cacop %1, %0,0x240; cacop %1, %0,0x260  ;  \n" \
     "   cacop %1, %0,0x280; cacop %1, %0,0x2a0  ;  \n" \
     "   cacop %1, %0,0x2c0; cacop %1, %0,0x2e0  ;  \n" \
     "   cacop %1, %0,0x300; cacop %1, %0,0x320  ;  \n" \
     "   cacop %1, %0,0x340; cacop %1, %0,0x360  ;  \n" \
     "   cacop %1, %0,0x380; cacop %1, %0,0x3a0  ;  \n" \
     "   cacop %1, %0,0x3c0; cacop %1, %0,0x3e0  ;  \n" \
         :                           \
         : "r" (base),                       \
           "i" (op));

/* build blast_xxx, blast_xxx_page, blast_xxx_page_indexed */
#define __BUILD_BLAST_CACHE(pfx, desc, indexop, hitop, lsize, extra)    \
 static inline void extra##blast_##pfx##cache##lsize(void)       \
 {                                   \
     unsigned long start = 0xa0000000 ;               \
     unsigned long end = start + current_cpu_data.desc.waysize;  \
     unsigned long ws_inc = 1UL << current_cpu_data.desc.waybit; \
     unsigned long ws_end = current_cpu_data.desc.ways <<        \
                    current_cpu_data.desc.waybit;        \
     unsigned long ws, addr;                     \
                                     \
     for (ws = 0; ws < ws_end; ws += ws_inc)             \
         for (addr = start; addr < end; addr += lsize * 32)  \
       cache##lsize##_unroll32(addr|ws, indexop);  \
 }                                   \
                                     \
 static inline void extra##blast_##pfx##cache##lsize##_page(unsigned long page) \
 {                                   \
     unsigned long start = page;                 \
     unsigned long end = page + PAGE_SIZE;               \
                                     \
     do {                                \
         cache##lsize##_unroll32(start, hitop);          \
         start += lsize * 32;                    \
     } while (start < end);                      \
 }                                   \
                                     \
static inline void extra##blast_##pfx##cache##lsize##_page_indexed(unsigned long page) \
 {                                   \
     unsigned long indexmask = current_cpu_data.desc.waysize - 1;    \
     unsigned long start = 0xa0000000 + (page & indexmask);      \
     unsigned long end = start + PAGE_SIZE;              \
     unsigned long ws_inc = 1UL << current_cpu_data.desc.waybit; \
     unsigned long ws_end = current_cpu_data.desc.ways <<        \
                    current_cpu_data.desc.waybit;        \
     unsigned long ws, addr;                     \
                                     \
     for (ws = 0; ws < ws_end; ws += ws_inc)             \
         for (addr = start; addr < end; addr += lsize * 32)  \
 cache##lsize##_unroll32(addr|ws, indexop);  \
 }

 __BUILD_BLAST_CACHE(d, dcache, Index_Writeback_Inv_D, Hit_Writeback_Inv_D, 16, )
 __BUILD_BLAST_CACHE(i, icache, Index_Invalidate_I, Hit_Invalidate_I, 16, )
 __BUILD_BLAST_CACHE(d, dcache, Index_Writeback_Inv_D, Hit_Writeback_Inv_D, 32, )
 __BUILD_BLAST_CACHE(i, icache, Index_Invalidate_I, Hit_Invalidate_I, 32, )

/* build blast_xxx_range, protected_blast_xxx_range */
 #define __BUILD_BLAST_CACHE_RANGE(pfx, desc, hitop, prot, extra)    \
 static inline void prot##extra##blast_##pfx##cache##_range(unsigned long start, \
                             unsigned long end)  \
 {                                   \
     unsigned long lsize = cpu_##desc##_line_size();         \
     unsigned long addr = start & ~(lsize - 1);          \
     unsigned long aend = (end - 1) & ~(lsize - 1);          \
                                     \
     while (1) {                         \
         prot##cache_op(hitop, addr);                \
         if (addr == aend)                   \
             break;                      \
         addr += lsize;                      \
     }                               \
 }

__BUILD_BLAST_CACHE_RANGE(d, dcache, Hit_Writeback_Inv_D, , )
__BUILD_BLAST_CACHE_RANGE(i, icache, Hit_Invalidate_I, , )
__BUILD_BLAST_CACHE_RANGE(inv_d, dcache, Hit_Writeback_Inv_D , , )

#endif
#endif /* _ASM_CACHEFLUSH_H */
