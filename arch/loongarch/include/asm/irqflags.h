/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020-2021 Loongson Technology Corporation Limited
 */
#ifndef _ASM_IRQFLAGS_H
#define _ASM_IRQFLAGS_H

#ifndef __ASSEMBLY__

#include <linux/compiler.h>
#include <linux/stringify.h>
#include <asm/compiler.h>
#include <asm/loongarchregs.h>

static inline void arch_local_irq_enable(void)
{
	csr_xchgl(CSR_CRMD_IE, CSR_CRMD_IE, LOONGARCH_CSR_CRMD);
}

static inline void arch_local_irq_disable(void)
{
	csr_xchgl(0, CSR_CRMD_IE, LOONGARCH_CSR_CRMD);
}

static inline unsigned long arch_local_irq_save(void)
{
	return csr_xchgl(0, CSR_CRMD_IE, LOONGARCH_CSR_CRMD);
}

static inline void arch_local_irq_restore(unsigned long flags)
{
	csr_xchgl(flags, CSR_CRMD_IE, LOONGARCH_CSR_CRMD);
}

static inline unsigned long arch_local_save_flags(void)
{
	return csr_readl(LOONGARCH_CSR_CRMD);
}

static inline int arch_irqs_disabled_flags(unsigned long flags)
{
	return !(flags & CSR_CRMD_IE);
}

static inline int arch_irqs_disabled(void)
{
	return arch_irqs_disabled_flags(arch_local_save_flags());
}
#endif /* #ifndef __ASSEMBLY__ */
#ifdef CONFIG_TRACE_IRQFLAGS
# define TRACE_IRQS_ON                                                  \
        CLI;    /* make sure trace_hardirqs_on() is called in kernel level */ \
        jal     trace_hardirqs_on
# define TRACE_IRQS_ON_RELOAD                                           \
        TRACE_IRQS_ON;                                                  \
        TRACE_IRQS_RELOAD_REGS
# define TRACE_IRQS_OFF                                                 \
        bl     trace_hardirqs_off
#else
# define TRACE_IRQS_ON
# define TRACE_IRQS_ON_RELOAD
# define TRACE_IRQS_OFF
#endif




#endif /* _ASM_IRQFLAGS_H */
