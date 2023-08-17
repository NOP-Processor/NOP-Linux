/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020-2021 Loongson Technology Corporation Limited
 */
#include <asm/asm.h>
#include <asm/export.h>
#include <asm/loongarchregs.h>
#include <asm/page.h>
#include <asm/pgtable-bits.h>
#include <asm/regdef.h>
#include <asm/stackframe.h>

#include <asm/pgtable-32.h>

	.macro tlb_do_page_fault, write
	SYM_FUNC_START(tlb_do_page_fault_\write)
	SAVE_ALL
	csrrd	a2, LOONGARCH_CSR_BADV
	KMODE
	move	a0, sp
	REG_S	a2, sp, PT_BVADDR
	li.w	a1, \write
	la.abs	t0, do_page_fault
	jirl    ra, t0, 0
	la.abs	t0, ret_from_exception
	jirl    zero, t0, 0
	SYM_FUNC_END(tlb_do_page_fault_\write)
	.endm

SYM_FUNC_START(handle_tlb_rixi)
	csrwr	t0, EXCEPTION_KS0
	csrwr	t1, EXCEPTION_KS1
SYM_FUNC_END(handle_tlb_rixi)
	/* Go through */
	tlb_do_page_fault 0
	tlb_do_page_fault 1


SYM_FUNC_START(handle_tlb_load)
	csrwr	ra, EXCEPTION_KS2

	lu12i.w	ra, 0xc
	slli.w	ra, ra, 0x10
	csrrd	t0, LOONGARCH_CSR_BADV
	bgeu	t0, ra, vmalloc_load

	csrrd	t1, LOONGARCH_CSR_PGD

vmalloc_done_load:
	/* Get PGD offset in bytes */
	srli.w	t0, t0, 0x16
	andi	t0, t0,	0x3ff
	slli.w	t0, t0, 0x2
	add.w	t1, t1, t0
	csrrd	t0, LOONGARCH_CSR_BADV
	ld.w	t1, t1, 0
	srli.w	t0, t0, 0xc
	andi	t0, t0, 0x3ff
	slli.w	t0, t0, 0x2
	add.w	t0, t0, t1

label_smp_pgtable_load:
	ld.w	t1, t0, 0
	tlbsrch

	srli.w  ra, t1, _PAGE_PRESENT_SHIFT
	andi    ra, ra, 1
	beq     ra, zero, nopage_tlb_load


	ori	t1, t1, _PAGE_VALID
	st.w	t1, t0, 0

	ori	t0, t0, 0x4
	xori	t0, t0, 0x4

	ld.w	t1, t0, 0
	srli.w	ra, t1, 0xc
	slli.w	ra, ra, 0x8
	andi	t1, t1, 0xff
	add.w	t1, t1, ra
	csrwr	t1, LOONGARCH_CSR_TLBELO0

	ld.w	t0, t0, 0x4
	srli.w  ra, t0, 0xc
        slli.w  ra, ra, 0x8
        andi    t0, t0, 0xff
        add.w   t0, t0, ra
	csrwr	t0, LOONGARCH_CSR_TLBELO1

	tlbwr

	csrrd	t0, LOONGARCH_CSR_KS0
	csrrd	t1, LOONGARCH_CSR_KS1
	csrrd	ra, EXCEPTION_KS2
	ertn

vmalloc_load:
	la.abs  t1, swapper_pg_dir
	b       vmalloc_done_load

	/*
	 * This is the entry point when build_tlbchange_handler_head
	 * spots a huge page.
	 */

nopage_tlb_load:
	dbar	0
        csrrd   ra, EXCEPTION_KS2
	la.abs	t0, tlb_do_page_fault_0
	jirl	$r0, t0, 0

SYM_FUNC_END(handle_tlb_load)




SYM_FUNC_START(handle_tlb_store)
	csrwr	ra, EXCEPTION_KS2

	lu12i.w ra, 0xc
        slli.w  ra, ra, 0x10
	csrrd	t0, LOONGARCH_CSR_BADV
	bgeu    t0, ra, vmalloc_store

	csrrd	t1, LOONGARCH_CSR_PGD

vmalloc_done_store:
	/* Get PGD offset in bytes */
	srli.w	t0, t0, 0x16
	andi	t0, t0, 0x3ff
	slli.w	t0, t0, 2
	add.w	t1, t1, t0
	csrrd	t0, LOONGARCH_CSR_BADV
	ld.w	t1, t1, 0
	srli.w	t0, t0, 0xc
	andi	t0, t0, 0x3ff
	slli.w	t0, t0, 0x2
	add.w	t1, t1, t0

label_smp_pgtable_store:
	ld.w	t0, t1, 0
	tlbsrch

	srli.w  ra, t0, _PAGE_PRESENT_SHIFT
        andi    ra, ra, ((_PAGE_PRESENT | _PAGE_WRITE) >> _PAGE_PRESENT_SHIFT)
	xori    ra, ra, ((_PAGE_PRESENT | _PAGE_WRITE) >> _PAGE_PRESENT_SHIFT)
	bne	ra, $r0, nopage_tlb_store

	ori	t0, t0, (_PAGE_VALID | _PAGE_DIRTY)
	st.w	t0, t1, 0

	ori	t1, t1, 4
	xori	t1, t1, 4

	ld.w	t0, t1, 0
	srli.w  ra, t0, 0xc
        slli.w  ra, ra, 0x8
        andi    t0, t0, 0xff
        add.w   t0, t0, ra
	csrwr   t0, LOONGARCH_CSR_TLBELO0

	ld.w	t1, t1, 4
	srli.w  ra, t1, 0xc
        slli.w  ra, ra, 0x8
        andi    t1, t1, 0xff
        add.w   t1, t1, ra
	csrwr	t1, LOONGARCH_CSR_TLBELO1

	tlbwr

	csrrd   t0, LOONGARCH_CSR_KS0
	csrrd	t1, LOONGARCH_CSR_KS1
	csrrd	ra, EXCEPTION_KS2
	ertn

vmalloc_store:
        la.abs  t1, swapper_pg_dir
        b       vmalloc_done_store

	/*
	 * This is the entry point when build_tlbchange_handler_head
	 * spots a huge page.
	 */

nopage_tlb_store:
	dbar	0
	csrrd	ra, EXCEPTION_KS2
	la.abs	t0, tlb_do_page_fault_1
	jirl	$r0, t0, 0

SYM_FUNC_END(handle_tlb_store)

SYM_FUNC_START(handle_tlb_modify)
	csrwr	ra, EXCEPTION_KS2

	/*
	 * The vmalloc handling is not in the hotpath.
	 */
	lu12i.w ra, 0xc
        slli.w  ra, ra, 0x10
        csrrd   t0, LOONGARCH_CSR_BADV
        bgeu    t0, ra, vmalloc_modify

	csrrd	t1, LOONGARCH_CSR_PGD

vmalloc_done_modify:
	/* Get PGD offset in bytes */
	srli.w	t0, t0, 0x16
	andi	t0, t0, 0x3ff
	slli.w	t0, t0, 2
	add.w	t1, t1, t0
	csrrd	t0, LOONGARCH_CSR_BADV
	ld.w	t1, t1, 0
	srli.w	t0, t0, 0xc
	andi	t0, t0, 0x3ff
	slli.w	t0, t0, 0x2
	add.w	t1, t1, t0

label_smp_pgtable_modify:
	ld.w	t0, t1, 0
	tlbsrch

	srli.w  ra, t0, _PAGE_WRITE_SHIFT
        andi    ra, ra, 1
	beq	ra, $r0, nopage_tlb_modify

	ori	t0, t0, (_PAGE_VALID | _PAGE_DIRTY)
	st.w	t0, t1, 0

	ori	t1, t1, 4
	xori	t1, t1, 4

	ld.w	t0, t1, 0
        srli.w  ra, t0, 0xc
        slli.w  ra, ra, 0x8
        andi    t0, t0, 0xff
        add.w   t0, t0, ra
	csrwr   t0, LOONGARCH_CSR_TLBELO0

	ld.w	t1, t1, 0x4
	srli.w  ra, t1, 0xc
        slli.w  ra, ra, 0x8
        andi    t1, t1, 0xff
        add.w   t1, t1, ra
	csrwr	t1, LOONGARCH_CSR_TLBELO1

	tlbwr

	csrrd	t0, LOONGARCH_CSR_KS0
	csrrd	t1, LOONGARCH_CSR_KS1
	csrrd	ra, EXCEPTION_KS2
	ertn

vmalloc_modify:
        la.abs  t1, swapper_pg_dir
        b       vmalloc_done_modify


nopage_tlb_modify:
	dbar	0
	csrrd	ra, EXCEPTION_KS2
	la.abs	t0, tlb_do_page_fault_1
	jirl	$r0, t0, 0

SYM_FUNC_END(handle_tlb_modify)

SYM_FUNC_START(handle_tlb_refill)
	csrwr	t0, LOONGARCH_CSR_KS0
	csrwr   t1, LOONGARCH_CSR_KS1
	csrwr   ra, EXCEPTION_KS2

	csrrd	t0, LOONGARCH_CSR_PGD
	csrrd   t1, LOONGARCH_CSR_BADV
	srli.w	t1, t1, 0x16
	slli.w	t1, t1, 0x2
	add.w	t0, t0, t1
	li.w	ra, 0xfffffff
	and	t0, t0, ra
	ld.w	t0, t0, 0

	csrrd	t1, LOONGARCH_CSR_BADV
	srli.w	t1, t1, 0xa
	andi	t1, t1, 0xff8
	add.w	t0, t0, t1
	and	t0, t0, ra

	ld.w	t1, t0, 0
	srli.w  ra, t1, 0xc
        slli.w  ra, ra, 0x8
        andi    t1, t1, 0xff
        add.w   t1, t1, ra
	csrwr	t1, LOONGARCH_CSR_TLBELO0

	ld.w	t1, t0, 0x4
	srli.w  ra, t1, 0xc
        slli.w  ra, ra, 0x8
        andi    t1, t1, 0xff
        add.w   t1, t1, ra
	csrwr	t1, LOONGARCH_CSR_TLBELO1

	tlbfill
	csrrd	t0, LOONGARCH_CSR_KS0
	csrrd   t1, LOONGARCH_CSR_KS1
	csrrd   ra, EXCEPTION_KS2
	ertn
SYM_FUNC_END(handle_tlb_refill)
