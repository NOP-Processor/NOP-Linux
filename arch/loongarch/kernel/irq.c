// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020-2021 Loongson Technology Corporation Limited
 */
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel_stat.h>
#include <linux/proc_fs.h>
#include <linux/mm.h>
#include <linux/random.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/kallsyms.h>
#include <linux/kgdb.h>
#include <linux/ftrace.h>

#include <linux/atomic.h>
#include <linux/uaccess.h>

DEFINE_PER_CPU(unsigned long, irq_stack);

/*
 * 'what should we do if we get a hw irq event on an illegal vector'.
 * each architecture has to answer this themselves.
 */
void ack_bad_irq(unsigned int irq)
{
	pr_warn("Unexpected IRQ # %d\n", irq);
}

atomic_t irq_err_count;

int arch_show_interrupts(struct seq_file *p, int prec)
{
#ifdef CONFIG_SMP
	show_ipi_list(p, prec);
#endif
	seq_printf(p, "%*s: %10u\n", prec, "ERR", atomic_read(&irq_err_count));
	return 0;
}

asmlinkage void spurious_interrupt(void)
{
	atomic_inc(&irq_err_count);
}

void __init init_IRQ(void)
{
	int i;
	unsigned int order = get_order(IRQ_STACK_SIZE);

	for (i = 0; i < NR_IRQS; i++)
		irq_set_noprobe(i);

	arch_init_irq();

	for_each_possible_cpu(i) {
		void *s = (void *)__get_free_pages(GFP_KERNEL, order);

		per_cpu(irq_stack, i) = (unsigned long)s;
		pr_debug("CPU%d IRQ stack at 0x%lx - 0x%lx\n", i,
			per_cpu(irq_stack, i), per_cpu(irq_stack, i) + IRQ_STACK_SIZE);
	}
}

#ifdef CONFIG_DEBUG_STACKOVERFLOW
static inline void check_stack_overflow(void)
{
	unsigned long sp;

	__asm__ __volatile__("move %0, $sp" : "=r" (sp));
	sp &= THREAD_MASK;

	/*
	 * Check for stack overflow: is there less than STACK_WARN free?
	 * STACK_WARN is defined as 1/8 of THREAD_SIZE by default.
	 */
	if (unlikely(sp < (sizeof(struct thread_info) + STACK_WARN))) {
		pr_warn("do_IRQ: stack overflow: %ld\n",
			sp - sizeof(struct thread_info));
		dump_stack();
	}
}
#else
static inline void check_stack_overflow(void) {}
#endif


/*
 * do_IRQ handles all normal device IRQ's (the special
 * SMP cross-CPU interrupts have their own specific
 * handlers).
 */
void __irq_entry do_IRQ(unsigned int irq)
{
	irq_enter();
	check_stack_overflow();
	generic_handle_irq(irq);
	irq_exit();
}
