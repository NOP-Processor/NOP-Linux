/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020-2021 Loongson Technology Corporation Limited
 */

#include <linux/linkage.h>

#include <asm/asm.h>
#include <asm/asmmacro.h>
#include <asm/cpu.h>
#include <asm/export.h>
#include <asm/regdef.h>
#include <uapi/asm-generic/errno-base.h>

#define _ASM_EXTABLE(from, to)                  \
        .section __ex_table, "a";               \
        PTR     from, to;                       \
        .previous

/*
 * unsigned long unaligned_read(void *value, void *addr, unsigned long n, bool symbol)
 *
 * a0: addr
 * a1: value
 * a2: n
 * a3: symbol
 */
SYM_FUNC_START(unaligned_read)
	beqz	a2, 4f

	li.w	t1, 8
	li.w	t2, 0

	addi.w	t0, a2, -1
	mul.w	t1, t0, t1
	add.w 	a0, a0, t0

	beq	a3, zero, 2f
1:	ld.b	t3, a0, 0
	b	3f

2:	ld.bu	t3, a0, 0
3:	sll.w	t3, t3, t1
	or	t2, t2, t3
	addi.w	t1, t1, -8
	addi.w	a2, a2, -1
	addi.w	a0, a0, -1
	bgt	a2, zero, 2b
4:	st.w	t2, a1, 0

	move	v0, a2
	jr	ra

5:	li.w    v0, -EFAULT
	jr	ra

	.section .fixup, "ax"
6:	li.w    v0, -EFAULT
	jr	ra
	.previous

	_ASM_EXTABLE(1b, 6b)
	_ASM_EXTABLE(2b, 6b)
	_ASM_EXTABLE(4b, 6b)
SYM_FUNC_END(unaligned_read)

/*
 * unsigned long unaligned_write(void *addr, unsigned long value, unsigned long n)
 *
 * a0: addr
 * a1: value
 * a2: n
 */
SYM_FUNC_START(unaligned_write)
	beqz	a2, 3f

	li.w	t0, 0
1:	srl.w	t1, a1, t0
2:	st.b	t1, a0, 0
	addi.w	t0, t0, 8
	addi.w	a2, a2, -1
	addi.w	a0, a0, 1
	bgt	a2, zero, 1b

	move	v0, a2
	jr	ra

3:	li.w    v0, -EFAULT
	jr	ra

	.section .fixup, "ax"
4:	li.w    v0, -EFAULT
	jr	ra
	.previous

	_ASM_EXTABLE(2b, 4b)
SYM_FUNC_END(unaligned_write)
