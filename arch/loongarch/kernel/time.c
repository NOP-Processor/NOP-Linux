// SPDX-License-Identifier: GPL-2.0
/*
 * Common time service routines for LoongArch machines.
 *
 * Copyright (C) 2020-2021 Loongson Technology Corporation Limited
 */
#include <linux/clockchips.h>
#include <linux/export.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/sched_clock.h>
#include <linux/spinlock.h>

#include <asm/cpu-features.h>
#include <asm/loongarchregs.h>
#include <asm/time.h>

u64 const_clock_freq;
EXPORT_SYMBOL(const_clock_freq);
u64 cpu_clock_freq;
EXPORT_SYMBOL(cpu_clock_freq);
static DEFINE_SPINLOCK(state_lock);
static DEFINE_PER_CPU(struct clock_event_device, constant_clockevent_device);

static void constant_event_handler(struct clock_event_device *dev)
{
}

irqreturn_t constant_timer_interrupt(int irq, void *data)
{
	int cpu = smp_processor_id();
	struct clock_event_device *cd;

	/* Clear Timer Interrupt */
	write_csr_tintclear(CSR_TINTCLR_TI);
	cd = &per_cpu(constant_clockevent_device, cpu);
	cd->event_handler(cd);

	return IRQ_HANDLED;
}

static int constant_set_state_oneshot(struct clock_event_device *evt)
{
	unsigned long timer_config;

	spin_lock(&state_lock);

	timer_config = csr_readq(LOONGARCH_CSR_TCFG);
	timer_config |=  CSR_TCFG_EN;
	timer_config &= ~CSR_TCFG_PERIOD;
	csr_writeq(timer_config, LOONGARCH_CSR_TCFG);

	spin_unlock(&state_lock);

	return 0;
}

static int constant_set_state_oneshot_stopped(struct clock_event_device *evt)
{
	return 0;
}

static int constant_set_state_periodic(struct clock_event_device *evt)
{
	u64 period;
	unsigned long timer_config;

	spin_lock(&state_lock);

	period = const_clock_freq;
	do_div(period, HZ);

	timer_config = period & CSR_TCFG_VAL;
	timer_config |= (CSR_TCFG_PERIOD | CSR_TCFG_EN);
	csr_writeq(timer_config, LOONGARCH_CSR_TCFG);

	spin_unlock(&state_lock);

	return 0;
}

static int constant_set_state_shutdown(struct clock_event_device *evt)
{
	return 0;
}

static int constant_timer_next_event(unsigned long delta, struct clock_event_device *evt)
{
	unsigned long timer_config;

	delta &= CSR_TCFG_VAL;
	timer_config = delta | CSR_TCFG_EN;
	csr_writeq(timer_config, LOONGARCH_CSR_TCFG);

	return 0;
}

static unsigned long __init get_loops_per_jiffy(void)
{
	u64 lpj = const_clock_freq;

	do_div(lpj, HZ);

	return lpj;
}

#ifdef CONFIG_SMP
/*
 * If we have a constant timer are using it for the delay loop, we can
 * skip clock calibration if another cpu in the same socket has already
 * been calibrated. This assumes that constant timer applies to all
 * cpus in the socket - this should be a safe assumption.
 */
unsigned long calibrate_delay_is_known(void)
{
	int next, cpu = smp_processor_id();
	const struct cpumask *mask = topology_core_cpumask(cpu);

	if (!mask)
		return 0;

	next = cpumask_any_but(mask, cpu);
	if (next < nr_cpu_ids)
		return cpu_data[next].udelay_val;

	return 0;
}
#endif

int constant_clockevent_init(void)
{
	unsigned int irq;
	unsigned int cpu = smp_processor_id();
	unsigned long min_delta = 0x600;
	unsigned long max_delta = (1ULL << 31) - 1;
	struct clock_event_device *cd;
	static int timer_irq_installed = 0;

	irq = LOONGSON_TIMER_IRQ;

	cd = &per_cpu(constant_clockevent_device, cpu);
	cd->name = "Constant";
	cd->features = CLOCK_EVT_FEAT_ONESHOT;
	cd->irq = irq;
	cd->rating = 320;
	cd->cpumask = cpumask_of(cpu);
	cd->set_state_oneshot = constant_set_state_oneshot;
	cd->set_state_oneshot_stopped = constant_set_state_oneshot_stopped;
	cd->set_state_periodic = constant_set_state_periodic;
	cd->set_state_shutdown = constant_set_state_shutdown;
	cd->set_next_event = constant_timer_next_event;
	cd->event_handler = constant_event_handler;

	clockevents_config_and_register(cd, const_clock_freq, min_delta, max_delta);

	if (timer_irq_installed)
		return 0;

	timer_irq_installed = 1;

	if (request_irq(irq, constant_timer_interrupt, IRQF_PERCPU | IRQF_TIMER, "timer", NULL))
		pr_err("Failed to request irq %d (timer)\n", irq);

	set_csr_ecfg(0x800);
	lpj_fine = get_loops_per_jiffy();
	pr_info("Constant clock event device register\n");

	return 0;
}

static u64 read_const_counter(struct clocksource *clk)
{
	return drdtime();
}

static struct clocksource clocksource_const = {
	.name = "Constant",
	.rating = 320,
	.read = read_const_counter,
	.mask = CLOCKSOURCE_MASK(64),
	.flags = CLOCK_SOURCE_IS_CONTINUOUS,
	.mult = 0,
	.shift = 10,
};

u64 native_sched_clock(void)
{
	return read_const_counter(NULL);
}

int __init constant_clocksource_init(void)
{
	int res;
	unsigned long freq;

	freq = const_clock_freq;

	clocksource_const.mult =
		clocksource_hz2mult(freq, clocksource_const.shift);

	res = clocksource_register_hz(&clocksource_const, freq);

	sched_clock_register(native_sched_clock, 64, freq);

	pr_info("Constant clock source device register\n");

	return res;
}

void __init time_init(void)
{
	if (!cpu_has_cpucfg)
		const_clock_freq = cpu_clock_freq;
	else
		const_clock_freq = calc_const_freq();

	constant_clockevent_init();
	constant_clocksource_init();
}
