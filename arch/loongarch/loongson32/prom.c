/*
 * LoongArch support for CONFIG_OF device tree support
 *
 * Copyright (C) 2010 Cisco Systems Inc. <dediao@cisco.com>
 * Copyright (C) 2020 Loongson Technology Corporation Limited
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/export.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/debugfs.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_platform.h>

#include <asm/bootinfo.h>
#include <asm/page.h>

void __init add_memory_region(phys_addr_t start, phys_addr_t size, long type)
{
    int x = boot_mem_map.nr_map;
    int i;

    /*
     * If the region reaches the top of the physical address space, adjust
     * the size slightly so that (start + size) doesn't overflow
     */
    if (start + size - 1 == PHYS_ADDR_MAX)
        --size;

    /* Sanity check */
    if (start + size < start) {
        pr_warn("Trying to add an invalid memory region, skipped\n");
        return;
    }

    /*
     * Try to merge with existing entry, if any.
     */
    for (i = 0; i < boot_mem_map.nr_map; i++) {
        struct boot_mem_map_entry *entry = boot_mem_map.map + i;
        unsigned long top;

        if (entry->type != type)
            continue;

        if (start + size < entry->addr)
            continue;           /* no overlap */

        if (entry->addr + entry->size < start)
            continue;           /* no overlap */

        top = max(entry->addr + entry->size, start + size);
        entry->addr = min(entry->addr, start);
        entry->size = top - entry->addr;

        return;
    }

    if (boot_mem_map.nr_map == BOOT_MEM_MAP_MAX) {
         pr_err("Ooops! Too many entries in the memory map!\n");
         return;
     }

     boot_mem_map.map[x].addr = start;
     boot_mem_map.map[x].size = size;
     boot_mem_map.map[x].type = type;
     boot_mem_map.nr_map++;


}

void __init early_init_dt_add_memory_arch(u64 base, u64 size)
{
	if (base >= PHYS_ADDR_MAX) {
		pr_warn("Trying to add an invalid memory region, skipped\n");
		return;
	}

	/* Truncate the passed memory region instead of type casting */
	if (base + size - 1 >= PHYS_ADDR_MAX || base + size < base) {
		pr_warn("Truncate memory region %llx @ %llx to size %llx\n",
			size, base, PHYS_ADDR_MAX - base);
		size = PHYS_ADDR_MAX - base;
	}

	add_memory_region(base, size, BOOT_MEM_RAM);
}

int __init early_init_dt_reserve_memory_arch(phys_addr_t base,
					phys_addr_t size, bool nomap)
{
	add_memory_region(base, size, BOOT_MEM_RESERVED);
	return 0;
}

void __init __dt_setup_arch(void *bph)
{
	if (!early_init_dt_scan(bph))
		return;
}

int __init __dt_register_buses(const char *bus0, const char *bus1)
{
	static struct of_device_id of_ids[3];

	if (!of_have_populated_dt())
		panic("device tree not present");

	strlcpy(of_ids[0].compatible, bus0, sizeof(of_ids[0].compatible));
	if (bus1) {
		strlcpy(of_ids[1].compatible, bus1,
			sizeof(of_ids[1].compatible));
	}

	if (of_platform_populate(NULL, of_ids, NULL, NULL))
		panic("failed to populate DT");

	return 0;
}
