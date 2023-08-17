/* SPDX-License-Identifier: GPL-2.0 */
/*
 * asmmacro.h: Assembler macros to make things easier to read.
 *
 * Copyright (C) 2020-2021 Loongson Technology Corporation Limited
 */
#ifndef _ASM_ASMMACRO_32_H
#define _ASM_ASMMACRO_32_H

#include <asm/asm-offsets.h>
#include <asm/regdef.h>
#include <asm/fpregdef.h>
#include <asm/loongarchregs.h>

	.macro	cpu_save_nonscratch thread
	st.w	s0, \thread, THREAD_REG23
	st.w	s1, \thread, THREAD_REG24
	st.w	s2, \thread, THREAD_REG25
	st.w	s3, \thread, THREAD_REG26
	st.w	s4, \thread, THREAD_REG27
	st.w	s5, \thread, THREAD_REG28
	st.w	s6, \thread, THREAD_REG29
	st.w	s7, \thread, THREAD_REG30
	st.w	s8, \thread, THREAD_REG31
	st.w	sp, \thread, THREAD_REG03
	st.w	fp, \thread, THREAD_REG22
	.endm

	.macro	cpu_restore_nonscratch thread
	ld.w	s0, \thread, THREAD_REG23
	ld.w	s1, \thread, THREAD_REG24
	ld.w	s2, \thread, THREAD_REG25
	ld.w	s3, \thread, THREAD_REG26
	ld.w	s4, \thread, THREAD_REG27
	ld.w	s5, \thread, THREAD_REG28
	ld.w	s6, \thread, THREAD_REG29
	ld.w	s7, \thread, THREAD_REG30
	ld.w	s8, \thread, THREAD_REG31
	ld.w	sp, \thread, THREAD_REG03
	ld.w	fp, \thread, THREAD_REG22
	ld.w	ra, \thread, THREAD_REG01
	.endm

#endif /* _ASM_ASMMACRO_32_H */
