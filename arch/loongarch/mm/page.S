/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020-2021 Loongson Technology Corporation Limited
 */
#include <linux/linkage.h>
#include <asm/asm.h>
#include <asm/export.h>
#include <asm/page.h>
#include <asm/regdef.h>

	.align 5
SYM_FUNC_START(clear_page)
	lu12i.w  t0, 1 << (PAGE_SHIFT - 12)
	LONG_ADDU    t0, t0, a0
1:
	LONG_S     zero, a0, 0
	LONG_S     zero, a0, 4
	LONG_S     zero, a0, 8
	LONG_S     zero, a0, 12
	LONG_S     zero, a0, 16
	LONG_S     zero, a0, 20
	LONG_S     zero, a0, 24
	LONG_S     zero, a0, 28

	LONG_ADDIU   a0,   a0, 32

	bne      t0,   a0, 1b

	jirl     $r0, ra, 0
SYM_FUNC_END(clear_page)


	.align 5
SYM_FUNC_START(copy_page)
	lu12i.w  t8, 1 << (PAGE_SHIFT - 12)
	LONG_ADDU    t8, t8, a0
1:
	LONG_L     t0, a1,  0
	LONG_L     t1, a1,  4
	LONG_L     t2, a1,  8
	LONG_L     t3, a1,  12
	LONG_L     t4, a1,  16
	LONG_L     t5, a1,  20
	LONG_L     t6, a1,  24
	LONG_L     t7, a1,  28

	LONG_S     t0, a0,  0
	LONG_S     t1, a0,  4
	LONG_S     t2, a0,  8
	LONG_S     t3, a0,  12
	LONG_S     t4, a0,  16
	LONG_S     t5, a0,  20
	LONG_S     t6, a0,  24
	LONG_S     t7, a0,  28

	LONG_ADDIU   a0, a0,  32
	LONG_ADDIU   a1, a1,  32

	bne      t8, a0, 1b
	jirl     $r0, ra, 0
SYM_FUNC_END(copy_page)
EXPORT_SYMBOL(copy_page)
