// SPDX-License-Identifier: GPL-2.0
/*
 * Author: Huacai Chen <chenhuacai@loongson.cn>
 * Copyright (C) 2020-2021 Loongson Technology Corporation Limited
 */
#include <linux/acpi.h>
#include <linux/dmi.h>
#include <linux/efi.h>
#include <linux/export.h>
#include <linux/init.h>
#include <linux/memblock.h>
#include <asm/acpi.h>
#include <asm/bootinfo.h>
#include <asm/cacheflush.h>
#include <asm/efi.h>
#include <asm/fw.h>
#include <asm/time.h>

#ifdef CONFIG_VT
#include <linux/console.h>
#include <linux/screen_info.h>
#include <linux/platform_device.h>
#endif

#include <loongson.h>
#include <linux/libfdt.h>
#include <linux/of_fdt.h>
#include <linux/of.h>

#define SMBIOS_BIOSSIZE_OFFSET		0x09
#define SMBIOS_BIOSEXTERN_OFFSET	0x13
#define SMBIOS_FREQLOW_OFFSET		0x16
#define SMBIOS_FREQHIGH_OFFSET		0x17
#define SMBIOS_FREQLOW_MASK		0xFF
#define SMBIOS_CORE_PACKAGE_OFFSET	0x23
#define LOONGSON_EFI_ENABLE		(1 << 3)
#define efi_table_attr(inst, attr)      (inst->attr)

struct loongson_board_info b_info;
static const char dmi_empty_string[] = "        ";
void *loongson_fdt_blob;
const efi_system_table_t *efi_system_table;

extern void __init memblock_remove_mem(void);
extern char __dtb_start[];
extern u32 __dtb_loongson32_ls_begin[];
extern void __init __dt_setup_arch(void *bph);
extern bool __init early_init_dt_verify(void *params);
extern inline u64 of_read_number(const __be32 *cell, int size);

struct loongsonlist_mem_map global_mem_map;
struct boot_mem_map boot_mem_map;

const char *get_system_type(void)
{
	return "generic-loongson-machine";
}

static int __init parse_cluster(struct device_node *cluster, int depth)
{
        char name[10];
        bool leaf = true;
        bool has_cores = false;
        struct device_node *c;
        int core_id = 0;
        int i, ret;

        i = 0;
        do {
                snprintf(name, sizeof(name), "cluster%d", i);
                c = of_get_child_by_name(cluster, name);
                if (c) {
                        leaf = false;
                        ret = parse_cluster(c, depth + 1);
                        of_node_put(c);
                        if (ret != 0)
                                return ret;
                }
                i++;
        } while (c);

        i = 0;
        do {
                snprintf(name, sizeof(name), "core%d", i);
                c = of_get_child_by_name(cluster, name);
                if (c) {
                        has_cores = true;

                        if (depth == 0) {
                                pr_err("%pOF: cpu-map children should be clusters\n",
                                       c);
                                of_node_put(c);
                                return -EINVAL;
                        }

                        if (leaf) {
                                core_id++;
                        } else {
                                pr_err("%pOF: Non-leaf cluster with core %s\n",
                                       cluster, name);
                                ret = -EINVAL;
                        }
                        of_node_put(c);
                        if (ret != 0)
                                return ret;
                }
                i++;
        } while (c);

        if (leaf && !has_cores)
                pr_warn("%pOF: empty cluster\n", cluster);

        if (loongson_sysconf.cores_per_package == 0)
                loongson_sysconf.cores_per_package = core_id;

        return 0;
}

static const char *dmi_string_parse(const struct dmi_header *dm, u8 s)
{
	const u8 *bp = ((u8 *) dm) + dm->length;

	if (s) {
		s--;
		while (s > 0 && *bp) {
			bp += strlen(bp) + 1;
			s--;
		}

		if (*bp != 0) {
			size_t len = strlen(bp)+1;
			size_t cmp_len = len > 8 ? 8 : len;

			if (!memcmp(bp, dmi_empty_string, cmp_len))
				return dmi_empty_string;

			return bp;
		}
	}

	return "";
}

static void __init parse_cpu_table(const struct dmi_header *dm)
{
	long freq_temp = 0;
	char *dmi_data = (char *)dm;

	freq_temp = ((*(dmi_data + SMBIOS_FREQHIGH_OFFSET) << 8) +
			((*(dmi_data + SMBIOS_FREQLOW_OFFSET)) & SMBIOS_FREQLOW_MASK));
	cpu_clock_freq = freq_temp * 1000000;

	loongson_sysconf.cpuname = (void *)dmi_string_parse(dm, dmi_data[16]);
	loongson_sysconf.cores_per_package = *(dmi_data + SMBIOS_CORE_PACKAGE_OFFSET);

	pr_info("CpuClock = %llu\n", cpu_clock_freq);
}

static void __init parse_bios_table(const struct dmi_header *dm)
{
	int bios_extern;
	char *dmi_data = (char *)dm;

	bios_extern = *(dmi_data + SMBIOS_BIOSEXTERN_OFFSET);
	b_info.bios_size = *(dmi_data + SMBIOS_BIOSSIZE_OFFSET);

	if (bios_extern & LOONGSON_EFI_ENABLE)
		set_bit(EFI_BOOT, &efi.flags);
	else
		clear_bit(EFI_BOOT, &efi.flags);
}

static void __init find_tokens(const struct dmi_header *dm, void *dummy)
{
	switch (dm->type) {
	case 0x0: /* Extern BIOS */
		parse_bios_table(dm);
		break;
	case 0x4: /* Calling interface */
		parse_cpu_table(dm);
		break;
	}
}

static void __init smbios_parse(void)
{
	b_info.bios_vendor = (void *)dmi_get_system_info(DMI_BIOS_VENDOR);
	b_info.bios_version = (void *)dmi_get_system_info(DMI_BIOS_VERSION);
	b_info.bios_release_date = (void *)dmi_get_system_info(DMI_BIOS_DATE);
	b_info.board_vendor = (void *)dmi_get_system_info(DMI_BOARD_VENDOR);
	b_info.board_name = (void *)dmi_get_system_info(DMI_BOARD_NAME);
	dmi_walk(find_tokens, NULL);
}

void __init early_init(void)
{
	fw_init_cmdline();
	fw_init_environ();
	early_memblock_init();
}

#define INVALID_HWID    0xFFFF
static u64 __init of_get_hwid(struct device_node *dn)
{
        const __be32 *cell = of_get_property(dn, "reg", NULL);

	if (!cell) {
		pr_err("%pOF: missing reg property\n", dn);
		return INVALID_HWID;
	}

	return of_read_number(cell, of_n_addr_cells(dn));
}

static int __init parse_dt_topology(void)
{
        struct device_node *cn, *map;
        int ret = 0;

        cn = of_find_node_by_path("/cpus");
        if (!cn) {
                return 0;
        }

        map = of_get_child_by_name(cn, "cpu-map");
        if (!map)
                goto out;

        ret = parse_cluster(map, 0);
        if (ret != 0)
                goto out_map;

out_map:
        of_node_put(map);
out:
        of_node_put(cn);
        return ret;
}

static void __init parse_dt_cpus(void)
{
        struct device_node *dn;
        int i;
        int nid = 0;
        int hwids[NR_CPUS];
        nodemask_t nodes_mask;
        int nr_nodes;

        loongson_sysconf.reserved_cpus_mask = -1;
        loongson_sysconf.boot_cpu_id = read_csr_cpuid();

        nodes_clear(nodes_mask);
        for_each_node_by_type(dn, "cpu") {
                u64 hwid = of_get_hwid(dn);

                if (hwid >= INVALID_HWID)
                        continue;

                for (i = 0; i < loongson_sysconf.nr_cpus; i++) {
                        if (hwids[i] == hwid) {
                                pr_err("%pOF: duplicate cpu reg properties in the DT\n", dn);
                                continue;
                        }
                }

                nid = of_node_to_nid(dn);
                if (nid != NUMA_NO_NODE)
                        node_set(nid, nodes_mask);

                if (of_node_to_nid(dn) == 0)
                        loongson_sysconf.cores_per_node++;

                if (loongson_sysconf.nr_cpus >= NR_CPUS)
                        break;

                hwids[loongson_sysconf.nr_cpus] = hwid;
                loongson_sysconf.reserved_cpus_mask &= (~(1 << hwid));
                loongson_sysconf.nr_cpus++;
        }
        nr_nodes = nodes_weight(nodes_mask);
        if (nr_nodes)
                loongson_sysconf.nr_nodes = nodes_weight(nodes_mask);
}

static void loongson_mem_init(void)
{
        int i;

        memset(&global_mem_map, 0, sizeof(global_mem_map));
        global_mem_map.map_count = boot_mem_map.nr_map;
        for (i = 0; i < boot_mem_map.nr_map; i++) {
                struct boot_mem_map_entry *bm_entry = boot_mem_map.map + i;
                struct loongson_mem_map *lm_entry = global_mem_map.map + i;

                if (bm_entry->type == 1)
                        lm_entry->mem_type = ADDRESS_TYPE_SYSRAM;
                else
                        lm_entry->mem_type = ADDRESS_TYPE_RESERVED;

                lm_entry->mem_start = bm_entry->addr;
                lm_entry->mem_size = bm_entry->size;
        }

        if (global_mem_map.map_count == 0)
                panic("No memory available!");
}

void __init device_tree_init(void)
{
        if (!initial_boot_params)
                return;

        if (early_init_dt_verify(initial_boot_params))
                unflatten_and_copy_device_tree();
}


void __init platform_init(void)
{
	//unsigned long fdt_addr, fdt_size;
	/* init base address of io space */
	set_io_port_base((unsigned long)
		ioremap(LOONGSON_LIO_BASE, LOONGSON_LIO_SIZE));
	efi_init();
#ifdef CONFIG_ACPI_TABLE_UPGRADE
	acpi_table_upgrade();
#endif

	loongson_fdt_blob = __dtb_loongson32_ls_begin;
	__dt_setup_arch(loongson_fdt_blob);
	early_init_fdt_reserve_self();
	early_init_fdt_scan_reserved_mem();

	loongson_mem_init();
	loongson_mem_map = &global_mem_map;
	early_memblock_init();
	device_tree_init();
	parse_dt_cpus();
	parse_dt_topology();

	fw_init_memory();
	dmi_setup();
	smbios_parse();
	pr_info("The BIOS Version: %s\n", b_info.bios_version);

	efi_runtime_init();

}

static int __init register_gop_device(void)
{
	void *pd;

	if (screen_info.orig_video_isVGA != VIDEO_TYPE_EFI)
		return 0;
	pd = platform_device_register_data(NULL, "efi-framebuffer", 0,
			&screen_info, sizeof(screen_info));
	return PTR_ERR_OR_ZERO(pd);
}
subsys_initcall(register_gop_device);
