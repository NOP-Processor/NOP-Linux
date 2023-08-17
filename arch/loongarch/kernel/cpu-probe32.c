// SPDX-License-Identifier: GPL-2.0
/*
 * Processor capabilities determination functions.
 *
 * Copyright (C) 2020-2021 Loongson Technology Corporation Limited
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/ptrace.h>
#include <linux/smp.h>
#include <linux/stddef.h>
#include <linux/export.h>
#include <linux/printk.h>
#include <linux/uaccess.h>

#include <asm/cpu.h>
#include <asm/cpu-features.h>
#include <asm/fpu.h>
#include <asm/loongarchregs.h>
#include <asm/elf.h>
#include <asm/pgtable-bits.h>

/* Hardware capabilities */
unsigned int elf_hwcap __read_mostly;
EXPORT_SYMBOL_GPL(elf_hwcap);

/*
 * Determine the FCSR mask for FPU hardware.
 */
static inline void cpu_set_fpu_fcsr_mask(struct cpuinfo_loongarch *c)
{
	unsigned long sr, mask, fcsr, fcsr0, fcsr1;

	fcsr = c->fpu_csr0;
	mask = FPU_CSR_ALL_X | FPU_CSR_ALL_E | FPU_CSR_ALL_S | FPU_CSR_RM;

	sr = read_csr_euen();
	enable_fpu();

	fcsr0 = fcsr & mask;
	write_fcsr(LOONGARCH_FCSR0, fcsr0);
	fcsr0 = read_fcsr(LOONGARCH_FCSR0);

	fcsr1 = fcsr | ~mask;
	write_fcsr(LOONGARCH_FCSR0, fcsr1);
	fcsr1 = read_fcsr(LOONGARCH_FCSR0);

	write_fcsr(LOONGARCH_FCSR0, fcsr);

	write_csr_euen(sr);

	c->fpu_mask = ~(fcsr0 ^ fcsr1) & ~mask;
}

static inline void set_elf_platform(int cpu, const char *plat)
{
	if (cpu == 0)
		__elf_platform = plat;
}

/* MAP BASE */
unsigned long vm_map_base;
EXPORT_SYMBOL_GPL(vm_map_base);

static void cpu_probe_addrbits(struct cpuinfo_loongarch *c)
{
#ifdef __NEED_ADDRBITS_PROBE
	c->pabits = (read_cpucfg(LOONGARCH_CPUCFG1) & CPUCFG1_PABITS) >> 4;
	c->vabits = (read_cpucfg(LOONGARCH_CPUCFG1) & CPUCFG1_VABITS) >> 12;
	vm_map_base = 0UL - (1UL << c->vabits);
#endif
	c->pabits = 31;
	c->vabits = 31;
	vm_map_base = 0UL - (1UL << c->vabits);
}

static void set_isa(struct cpuinfo_loongarch *c, unsigned int isa)
{
	switch (isa) {
	case LOONGARCH_CPU_ISA_LA32S:
		c->isa_level |= LOONGARCH_CPU_ISA_LA32S;
		fallthrough;
	case LOONGARCH_CPU_ISA_LA32R:
		c->isa_level |= LOONGARCH_CPU_ISA_LA32R;
		break;
	}
}

static void decode_configs(struct cpuinfo_loongarch *c)
{
        unsigned long asid_mask;

	asid_mask =0xff;
	set_cpu_asid_mask(c, asid_mask);
	c->tlbsizemtlb = 0x40;
	c->tlbsizestlbsets = 0x100;
	c->tlbsizestlbways = 0x8;
	c->tlbsize = c->tlbsizemtlb + c->tlbsizestlbsets*  c->tlbsizestlbways;
}

#define MAX_NAME_LEN	32
#define VENDOR_OFFSET	0
#define CPUNAME_OFFSET	9

static char cpu_full_name[MAX_NAME_LEN] = "        -        ";

static inline void cpu_probe_loongson(struct cpuinfo_loongarch *c, unsigned int cpu)
{
	c->options = LOONGARCH_CPU_CPUCFG | LOONGARCH_CPU_CSR |
		     LOONGARCH_CPU_TLB | LOONGARCH_CPU_VINT | LOONGARCH_CPU_WATCH;

	decode_configs(c);
	elf_hwcap |= HWCAP_LOONGARCH_CRC32;

	__cpu_full_name[cpu] = cpu_full_name;

	switch (c->processor_id & PRID_IMP_MASK) {
	case PRID_IMP_LOONGSON_32:
		c->cputype = CPU_LOONGSON32;
		set_isa(c, LOONGARCH_CPU_ISA_LA32S);
		__cpu_family[cpu] = "Loongson-32bit";
		pr_info("Standard 32-bit Loongson Processor probed\n");
		break;
	default: // Default to 64 bit
		c->cputype = CPU_LOONGSON64;
		set_isa(c, LOONGARCH_CPU_ISA_LA64);
		__cpu_family[cpu] = "Loongson-64bit";
		pr_info("Unknown 64-bit Loongson Processor probed\n");
	}
}



/* For use by uaccess.h */
u32 __ua_limit;
EXPORT_SYMBOL(__ua_limit);


const char *__cpu_family[NR_CPUS];
const char *__cpu_full_name[NR_CPUS];
const char *__elf_platform;

void cpu_probe(void)
{
	struct cpuinfo_loongarch *c = &current_cpu_data;
	unsigned int cpu = smp_processor_id();

	set_elf_platform(cpu, "loongarch");

	c->cputype	= CPU_UNKNOWN;
	c->processor_id = 0x4200;

	c->fpu_csr0	= FPU_CSR_RN;
	c->fpu_mask	= FPU_CSR_RSVD;

	switch (c->processor_id & PRID_IMP_MASK) {
	case PRID_IMP_LOONGSON_32:
		cpu_probe_loongson(c, cpu);
		break;
	}

	BUG_ON(!__cpu_family[cpu]);
	BUG_ON(c->cputype == CPU_UNKNOWN);

	cpu_probe_addrbits(c);

	if (cpu == 0)
		__ua_limit = ~((1ul << cpu_vabits) - 1);
}

void cpu_report(void)
{
	struct cpuinfo_loongarch *c = &current_cpu_data;

	pr_info("CPU%d revision is: %08x (%s)\n",
		smp_processor_id(), c->processor_id, cpu_family_string());
	if (c->options & LOONGARCH_CPU_FPU)
		pr_info("FPU%d revision is: %08x\n", smp_processor_id(), c->fpu_vers);
}

void cpu_set_cluster(struct cpuinfo_loongarch *cpuinfo, unsigned int cluster)
{
	/* Ensure the core number fits in the field */
	WARN_ON(cluster > (LOONGARCH_GLOBALNUMBER_CLUSTER >>
			   LOONGARCH_GLOBALNUMBER_CLUSTER_SHF));

	cpuinfo->globalnumber &= ~LOONGARCH_GLOBALNUMBER_CLUSTER;
	cpuinfo->globalnumber |= cluster << LOONGARCH_GLOBALNUMBER_CLUSTER_SHF;
}

void cpu_set_core(struct cpuinfo_loongarch *cpuinfo, unsigned int core)
{
	/* Ensure the core number fits in the field */
	WARN_ON(core > (LOONGARCH_GLOBALNUMBER_CORE >> LOONGARCH_GLOBALNUMBER_CORE_SHF));

	cpuinfo->globalnumber &= ~LOONGARCH_GLOBALNUMBER_CORE;
	cpuinfo->globalnumber |= core << LOONGARCH_GLOBALNUMBER_CORE_SHF;
}
