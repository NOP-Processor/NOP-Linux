/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020-2021 Loongson Technology Corporation Limited
 */
#ifndef _ASM_BITOPS_H
#define _ASM_BITOPS_H

#ifndef _LINUX_BITOPS_H
#error only <linux/bitops.h> can be included directly
#endif

#include <linux/compiler.h>
#include <linux/types.h>
#include <asm/barrier.h>
#include <asm/byteorder.h>
#include <asm/compiler.h>
#include <asm/cpu-features.h>

#if _LOONGARCH_SZLONG == 32
#define __LL		"ll.w	"
#define __SC		"sc.w	"
#define __AMADD		"amadd.w	"
#define __AMAND_SYNC	"amand_db.w	"
#define __AMOR_SYNC	"amor_db.w	"
#define __AMXOR_SYNC	"amxor_db.w	"

#elif _LOONGARCH_SZLONG == 64
#define __LL		"ll.d	"
#define __SC		"sc.d	"
#define __AMADD		"amadd.d	"
#define __AMAND_SYNC	"amand_db.d	"
#define __AMOR_SYNC	"amor_db.d	"
#define __AMXOR_SYNC	"amxor_db.d	"
#endif

/*
 * set_bit - Atomically set a bit in memory
 * @nr: the bit to set
 * @addr: the address to start counting from
 *
 * This function is atomic and may not be reordered.  See __set_bit()
 * if you do not require the atomic guarantees.
 * Note that @nr may be almost arbitrarily large; this function is not
 * restricted to acting on a single-word quantity.
 */

static inline void set_bit(unsigned long nr, volatile unsigned long *addr)
{
#ifdef CONFIG_32BIT
	unsigned long temp;
#endif
	int bit = nr % BITS_PER_LONG;
	volatile unsigned long *m = &addr[BIT_WORD(nr)];

#ifdef CONFIG_64BIT
	__asm__ __volatile__(
	"   " __AMOR_SYNC "$zero, %1, %0        \n"
	: "+ZB" (*m)
	: "r" (1UL << bit)
	: "memory");
#else
	__asm__ __volatile__(
	"1:     " __LL "%0, %1                  \n"
	"       or      %0, %0, %2                      \n"
	"       " __SC  "%0, %1                 \n"
	"       beq     %0, $r0, 1b             \n"
	: "=&r" (temp), "=" GCC_OFF_SMALL_ASM() (*m)
	: "r" (1UL << bit));
#endif
}
/*
 * clear_bit - Clears a bit in memory
 * @nr: Bit to clear
 * @addr: Address to start counting from
 *
 * clear_bit() is atomic and may not be reordered.  However, it does
 * not contain a memory barrier, so if it is used for locking purposes,
 * you should call smp_mb__before_atomic() and/or smp_mb__after_atomic()
 * in order to ensure changes are visible on other processors.
 */
static inline void clear_bit(unsigned long nr, volatile unsigned long *addr)
{
#ifdef CONFIG_32BIT
	unsigned long temp;
#endif
	int bit = nr % BITS_PER_LONG;
	volatile unsigned long *m = &addr[BIT_WORD(nr)];

#ifdef CONFIG_64BIT
	__asm__ __volatile__(
	"   " __AMAND_SYNC "$zero, %1, %0       \n"
	: "+ZB" (*m)
	: "r" (~(1UL << bit))
	: "memory");
#else
        __asm__ __volatile__(
        "1:     " __LL "%0, %1                  \n"
        "       and     %0, %0, %2                      \n"
        "       " __SC  "%0, %1                 \n"
        "       beq     %0, $r0, 1b             \n"
        : "=&r" (temp), "=" GCC_OFF_SMALL_ASM() (*m)
        : "r" (~(1UL << bit)));
#endif
}

/*
 * clear_bit_unlock - Clears a bit in memory
 * @nr: Bit to clear
 * @addr: Address to start counting from
 *
 * clear_bit() is atomic and implies release semantics before the memory
 * operation. It can be used for an unlock.
 */
static inline void clear_bit_unlock(unsigned long nr, volatile unsigned long *addr)
{
	clear_bit(nr, addr);
}

/*
 * change_bit - Toggle a bit in memory
 * @nr: Bit to change
 * @addr: Address to start counting from
 *
 * change_bit() is atomic and may not be reordered.
 * Note that @nr may be almost arbitrarily large; this function is not
 * restricted to acting on a single-word quantity.
 */
static inline void change_bit(unsigned long nr, volatile unsigned long *addr)
{
#ifdef CONFIG_32BIT
        unsigned long temp;
#endif
	int bit = nr % BITS_PER_LONG;
	volatile unsigned long *m = &addr[BIT_WORD(nr)];

#ifdef CONFIG_64BIT
	__asm__ __volatile__(
	"   " __AMXOR_SYNC "$zero, %1, %0       \n"
	: "+ZB" (*m)
	: "r" (1UL << bit)
	: "memory");
#else
	__asm__ __volatile__(
	"1:     "__LL "%0, %1           \n"
	"       xor     %0, %0, %2      \n"
	"       "__SC "%0, %1           \n"
	"       beq     %0, $r0, 1b     \n"
	: "=&r" (temp), "+" GCC_OFF_SMALL_ASM() (*m)
	: "r" (1UL << bit));
#endif
}
/*
 * test_and_set_bit - Set a bit and return its old value
 * @nr: Bit to set
 * @addr: Address to count from
 *
 * This operation is atomic and cannot be reordered.
 * It also implies a memory barrier.
 */
static inline int test_and_set_bit(unsigned long nr,
	volatile unsigned long *addr)
{
#ifdef CONFIG_32BIT
	unsigned long temp;
#endif
	int bit = nr % BITS_PER_LONG;
	unsigned long res;
	volatile unsigned long *m = &addr[BIT_WORD(nr)];

#ifdef CONFIG_64BIT
	__asm__ __volatile__(
	"   " __AMOR_SYNC "%1, %2, %0       \n"
	: "+ZB" (*m), "=&r" (res)
	: "r" (1UL << bit)
	: "memory");

	res = res & (1UL << bit);
#else
__asm__ __volatile__(
	"1:     "__LL  "%0, %1          \n"
	"       or      %2, %0, %3      \n"
	"       "__SC   "%2, %1         \n"
	"       beq     %2, $r0, 1b     \n"
	: "=&r" (temp), "+" GCC_OFF_SMALL_ASM() (*m), "=&r" (res)
	: "r" (1UL << bit)
	: "memory");
	res = temp & (1UL << bit);
#endif

	return res != 0;
}
/*
 * test_and_set_bit_lock - Set a bit and return its old value
 * @nr: Bit to set
 * @addr: Address to count from
 *
 * This operation is atomic and implies acquire ordering semantics
 * after the memory operation.
 */
static inline int test_and_set_bit_lock(unsigned long nr,
	volatile unsigned long *addr)
{
#ifdef CONFIG_32BIT
	unsigned long temp;
#endif
	int bit = nr % BITS_PER_LONG;
	unsigned long res;
	volatile unsigned long *m = &addr[BIT_WORD(nr)];

#ifdef CONFIG_64BIT
	__asm__ __volatile__(
	"   " __AMOR_SYNC "%1, %2, %0       \n"
	: "+ZB" (*m), "=&r" (res)
	: "r" (1UL << bit)
	: "memory");

	res = res & (1UL << bit);
#else
        __asm__ __volatile__(
        "1:     " __LL "%0, %1                          \n"
        "       or      %2, %0, %3                      \n"
        "       " __SC  "%2, %1                         \n"
        "       beq %2, $r0, 1b                         \n"
        : "=&r" (temp), "+" GCC_OFF_SMALL_ASM() (*m), "=&r" (res)
        : "r" (1UL << bit)
        : "memory");

        res = temp & (1UL << bit);
#endif
	return res != 0;
}
/*
 * test_and_clear_bit - Clear a bit and return its old value
 * @nr: Bit to clear
 * @addr: Address to count from
 *
 * This operation is atomic and cannot be reordered.
 * It also implies a memory barrier.
 */
static inline int test_and_clear_bit(unsigned long nr,
	volatile unsigned long *addr)
{
	int bit = nr % BITS_PER_LONG;
	unsigned long res, temp;
	volatile unsigned long *m = &addr[BIT_WORD(nr)];

#ifdef CONFIG_64BIT
	__asm__ __volatile__(
	"   " __AMAND_SYNC "%1, %2, %0      \n"
	: "+ZB" (*m), "=&r" (temp)
	: "r" (~(1UL << bit))
	: "memory");
#else
        __asm__ __volatile__(
        "1:     " __LL  "%0, %1 # test_and_clear_bit    \n"
        "       or      %2, %0, %3                      \n"
        "       xor     %2, %2, %3                      \n"
        "       " __SC  "%2, %1                         \n"
        "       beq     %2, $r0, 1b                     \n"
        : "=&r" (temp), "+" GCC_OFF_SMALL_ASM() (*m), "=&r" (res)
        : "r" (1UL << bit)
        : "memory");
#endif
        res = temp & (1UL << bit);
	return res != 0;
}
/*
 * test_and_change_bit - Change a bit and return its old value
 * @nr: Bit to change
 * @addr: Address to count from
 *
 * This operation is atomic and cannot be reordered.
 * It also implies a memory barrier.
 */
static inline int test_and_change_bit(unsigned long nr,
	volatile unsigned long *addr)
{
#ifdef CONFIG_32BIT
        unsigned long temp;
#endif
	int bit = nr % BITS_PER_LONG;
	unsigned long res;
	volatile unsigned long *m = &addr[BIT_WORD(nr)];

#ifdef CONFIG_64BIT
	__asm__ __volatile__(
	"   " __AMXOR_SYNC "%1, %2, %0      \n"
	: "+ZB" (*m), "=&r" (res)
	: "r" (1UL << bit)
	: "memory");
	res = res & (1UL << bit);
#else
        __asm__ __volatile__(
        "1:     " __LL  "%0, %1 # test_and_change_bit   \n"
        "       xor     %2, %0, %3                      \n"
        "       " __SC  "\t%2, %1                       \n"
        "       beq     %2, $r0, 1b"
        : "=&r" (temp), "+" GCC_OFF_SMALL_ASM() (*m), "=&r" (res)
        : "r" (1UL << bit)
        : "memory");
#endif
	return res != 0;
}

#include <asm-generic/bitops/non-atomic.h>

/*
 * __clear_bit_unlock - Clears a bit in memory
 * @nr: Bit to clear
 * @addr: Address to start counting from
 *
 * __clear_bit() is non-atomic and implies release semantics before the memory
 * operation. It can be used for an unlock if no other CPUs can concurrently
 * modify other bits in the word.
 */
static inline void __clear_bit_unlock(unsigned long nr, volatile unsigned long *addr)
{
	clear_bit(nr, addr);
}

#ifdef CONFIG_64BIT
#include <asm-generic/bitops/builtin-ffs.h>
#include <asm-generic/bitops/builtin-fls.h>
#include <asm-generic/bitops/builtin-__ffs.h>
#include <asm-generic/bitops/builtin-__fls.h>
#endif

#ifdef CONFIG_32BIT
static inline int fls(int x)
{
        int r;

        r = 32;
        if (!x)
                return 0;
        if (!(x & 0xffff0000u)) {
                x <<= 16;
                r -= 16;
        }
        if (!(x & 0xff000000u)) {
                x <<= 8;
                r -= 8;
        }
        if (!(x & 0xf0000000u)) {
                x <<= 4;
                r -= 4;
        }
        if (!(x & 0xc0000000u)) {
                x <<= 2;
                r -= 2;
        }
        if (!(x & 0x80000000u)) {
                x <<= 1;
                r -= 1;
        }
        return r;
}

static inline int ffs(int word)
{
        if (!word)
                return 0;

        return fls(word & -word);
}

static inline unsigned long __fls(unsigned long word)
{
        int num;

        if (BITS_PER_LONG == 64 && !__builtin_constant_p(word)) {
                __asm__(
                "       clz.d   %0, %1                          \n"
                : "=r" (num)
                : "r" (word));

                return 63 - num;
        }

        num = BITS_PER_LONG - 1;
#if BITS_PER_LONG == 64
        if (!(word & (~0ul << 32))) {
                num -= 32;
                word <<= 32;
        }
#endif
        if (!(word & (~0ul << (BITS_PER_LONG-16)))) {
                num -= 16;
                word <<= 16;
        }
        if (!(word & (~0ul << (BITS_PER_LONG-8)))) {
                num -= 8;
                word <<= 8;
        }
        if (!(word & (~0ul << (BITS_PER_LONG-4)))) {
                num -= 4;
                word <<= 4;
        }
        if (!(word & (~0ul << (BITS_PER_LONG-2)))) {
                num -= 2;
                word <<= 2;
        }
        if (!(word & (~0ul << (BITS_PER_LONG-1))))
                num -= 1;
        return num;
}

static inline unsigned long __ffs(unsigned long word)
{
        return __fls(word & -word);
}
#endif

#include <asm-generic/bitops/ffz.h>
#include <asm-generic/bitops/fls64.h>
#include <asm-generic/bitops/find.h>
#ifdef __KERNEL__

#include <asm-generic/bitops/sched.h>

#include <asm-generic/bitops/arch_hweight.h>
#include <asm-generic/bitops/const_hweight.h>

#include <asm-generic/bitops/le.h>
#include <asm-generic/bitops/ext2-atomic.h>

#endif /* __KERNEL__ */

#endif /* _ASM_BITOPS_H */
