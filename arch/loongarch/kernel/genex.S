/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020-2021 Loongson Technology Corporation Limited
 */
#include <linux/init.h>

#include <asm/asm.h>
#include <asm/asmmacro.h>
#include <asm/cacheops.h>
#include <asm/irqflags.h>
#include <asm/regdef.h>
#include <asm/fpregdef.h>
#include <asm/loongarchregs.h>
#include <asm/stackframe.h>
#include <asm/thread_info.h>

	.align	5	/* 32 byte rollback region */
SYM_FUNC_START(__arch_cpu_idle)
	/* start of rollback region */
	LONG_L	t0, tp, TI_FLAGS
	nop
	andi	t0, t0, _TIF_NEED_RESCHED
	bnez	t0, 1f
	nop
	nop
	nop
	idle	0
	/* end of rollback region */
1:
	jirl	zero, ra, 0
SYM_FUNC_END(__arch_cpu_idle)

SYM_FUNC_START(except_vec_cex)
	b	cache_parity_error
	nop
SYM_FUNC_END(except_vec_cex)

	.macro	__build_clear_none
	.endm

	.macro	__build_clear_sti
	STI
	.endm

	.macro	__build_clear_cli
	CLI
	.endm

	.macro	__build_clear_fpe
	movfcsr2gr	a1, fcsr0
	CLI
	.endm

	.macro	__build_clear_ade
	csrrd	t0, LOONGARCH_CSR_BADV
	PTR_S	t0, sp, PT_BVADDR
	KMODE
	.endm

	.macro	__build_clear_ale
	csrrd	t0, LOONGARCH_CSR_BADV
	PTR_S	t0, sp, PT_BVADDR
	KMODE
	.endm

	.macro	__BUILD_silent exception
	.endm

	.macro	__BUILD_verbose nexception
	LONG_L	a1, sp, PT_EPC
	ASM_PRINT("Got \nexception at %016lx\012")
	.endm

	.macro	__BUILD_HANDLER exception handler clear verbose ext
	.align	5
	SYM_FUNC_START(handle_\exception)
	SAVE_ALL
	SYM_INNER_LABEL(handle_\exception\ext, SYM_L_GLOBAL)
	__build_clear_\clear
	__BUILD_\verbose \exception
	move	a0, sp
	la.abs	t0, do_\handler
	jirl    ra, t0, 0
	la.abs	t0, ret_from_exception
	jirl    zero, t0, 0
	SYM_FUNC_END(handle_\exception)
	.endm

	.macro	BUILD_HANDLER exception handler clear verbose
	__BUILD_HANDLER \exception \handler \clear \verbose _int
	.endm

	BUILD_HANDLER ade ade ade silent
	BUILD_HANDLER ale ale ale silent
	BUILD_HANDLER bp bp sti silent
	BUILD_HANDLER ri ri sti silent
	BUILD_HANDLER fpu fpu sti silent
	BUILD_HANDLER fpe fpe fpe silent
	BUILD_HANDLER lsx lsx sti silent
	BUILD_HANDLER lasx lasx sti silent
	BUILD_HANDLER lbt lbt sti silent
	BUILD_HANDLER watch watch cli silent
	BUILD_HANDLER reserved reserved sti verbose	/* others */

SYM_FUNC_START(handle_syscall)
	la.abs	t0, handle_sys
	jirl    zero, t0, 0
SYM_FUNC_END(handle_syscall)

/*
 * Common Vectored Interrupt
 * Complete the register saves and invoke the do_vi() handler
 */

SYM_FUNC_START(except_vec_vi_handler)

	BACKUP_T0T1
	csrrd	t0, LOONGARCH_CSR_ESTAT
	srli.w	t1, t0, 0x10   /* get Ecode */
	andi	t1, t1, 0x3f
	beq	t1, zero, 1f	/* if irq */

        csrrd	t0, LOONGARCH_CSR_EENTRY
        slli.w	t1, t1, 0x9  /* get ex entry shift = Ecode * vec_size  */
        add.w	t0, t0, t1
        lu12i.w	t1, 0x4
        sub.w	t0, t0, t1
        jirl	zero,t0 , 0  /* go to exception_handler */

1:
	SAVE_SOME
	la.abs	t1, __arch_cpu_idle
	LONG_L	t0, sp, PT_EPC

	/* 32 byte rollback region */
	ori	t0, t0, 0x1f
	xori	t0, t0, 0x1f
	bne	t0, t1, 2f
	LONG_S	t0, sp, PT_EPC
2:
	SAVE_TEMP
	SAVE_STATIC

	CLI
	TRACE_IRQS_OFF

	ld.w	s0, tp, TI_REGS
	st.w	sp, tp, TI_REGS
	move	s1, sp

	csrrd	t0, LOONGARCH_CSR_TMID
	la.abs	t1, irq_stack
	slli.w	t0, t0, LONGLOG
	ld.w	t0, t1, 0

	li.w	t1, ~(_THREAD_SIZE-1)
	and	t1, t1, sp
	beq	t0, t1, 2f

	li.w	t1, _IRQ_STACK_START
	add.w	sp, t0, t1
	st.w	s1,sp, 0

2:
	la.abs	t0, plat_irq_dispatch
	jirl	ra, t0, 0

	move	sp, s1

	la.abs	t0, ret_from_irq
	jirl	zero, t0, 0
SYM_FUNC_END(except_vec_vi_handler)


	.macro	BUILD_VI_HANDLER num
	.align	5
SYM_FUNC_START(handle_vi_\num)
	BACKUP_T0T1
	SAVE_SOME
	addi.d	v0, zero, \num
	la.abs	v1, except_vec_vi_handler
	jirl	zero, v1, 0
SYM_FUNC_END(handle_vi_\num)
	.endm

	BUILD_VI_HANDLER 0
	BUILD_VI_HANDLER 1
	BUILD_VI_HANDLER 2
	BUILD_VI_HANDLER 3
	BUILD_VI_HANDLER 4
	BUILD_VI_HANDLER 5
	BUILD_VI_HANDLER 6
	BUILD_VI_HANDLER 7
	BUILD_VI_HANDLER 8
	BUILD_VI_HANDLER 9
	BUILD_VI_HANDLER 10
	BUILD_VI_HANDLER 11
	BUILD_VI_HANDLER 12
	BUILD_VI_HANDLER 13

	.align	3
SYM_DATA_START(vi_table)
	PTR	handle_vi_0
	PTR	handle_vi_1
	PTR	handle_vi_2
	PTR	handle_vi_3
	PTR	handle_vi_4
	PTR	handle_vi_5
	PTR	handle_vi_6
	PTR	handle_vi_7
	PTR	handle_vi_8
	PTR	handle_vi_9
	PTR	handle_vi_10
	PTR	handle_vi_11
	PTR	handle_vi_12
	PTR	handle_vi_13
SYM_DATA_END(vi_table)
