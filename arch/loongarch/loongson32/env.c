// SPDX-License-Identifier: GPL-2.0
/*
 * Author: Huacai Chen <chenhuacai@loongson.cn>
 *
 * Copyright (C) 2020-2021 Loongson Technology Corporation Limited
 */
#include <linux/export.h>
#include <linux/acpi.h>
#include <linux/efi.h>
#include <asm/fw.h>
#include <asm/time.h>
#include <asm/bootinfo.h>
#include <loongson.h>

struct bootparamsinterface *efi_bp;
struct loongsonlist_mem_map *loongson_mem_map;
struct loongsonlist_vbios *pvbios;
struct loongson_system_configuration loongson_sysconf;
EXPORT_SYMBOL(loongson_sysconf);

u32 loongson_chipcfg[MAX_PACKAGES] = {0xbfc00180};
u32 loongson_chiptemp[MAX_PACKAGES];
u32 loongson_freqctrl[MAX_PACKAGES];
unsigned long long smp_group[MAX_PACKAGES];

static void __init register_addrs_set(u32 *registers, const u32 addr, int num)
{
	u64 i;

	for (i = 0; i < num; i++) {
		*registers = (i << 44) | addr;
		registers++;
	}
}

static u8 ext_listhdr_checksum(u8 *buffer, u32 length)
{
	u8 sum = 0;
	u8 *end = buffer + length;

	while (buffer < end) {
		sum = (u8)(sum + *(buffer++));
	}

	return (sum);
}

static int parse_mem(struct _extention_list_hdr *head)
{
	loongson_mem_map = (struct loongsonlist_mem_map *)head;
	if (ext_listhdr_checksum((u8 *)loongson_mem_map, head->length)) {
		pr_warn("mem checksum error\n");
		return -EPERM;
	}

	return 0;
}

static int parse_vbios(struct _extention_list_hdr *head)
{
	pvbios = (struct loongsonlist_vbios *)head;

	if (ext_listhdr_checksum((u8 *)pvbios, head->length)) {
		pr_warn("vbios_addr checksum error\n");
		return -EPERM;
	}

	loongson_sysconf.vgabios_addr = pvbios->vbios_addr;

	return 0;
}

static int parse_screeninfo(struct _extention_list_hdr *head)
{
	struct loongsonlist_screeninfo *pscreeninfo;

	pscreeninfo = (struct loongsonlist_screeninfo *)head;
	if (ext_listhdr_checksum((u8 *)pscreeninfo, head->length)) {
		pr_warn("screeninfo_addr checksum error\n");
		return -EPERM;
	}

	memcpy(&screen_info, &pscreeninfo->si, sizeof(screen_info));

	return 0;
}

static int list_find(struct _extention_list_hdr *head)
{
	struct _extention_list_hdr *fhead = head;

	if (fhead == NULL) {
		pr_warn("the link is empty!\n");
		return -1;
	}

	while (fhead != NULL) {
		if (memcmp(&(fhead->signature), LOONGSON_MEM_LINKLIST, 3) == 0) {
			if (parse_mem(fhead) != 0) {
				pr_warn("parse mem failed\n");
				return -EPERM;
			}
		} else if (memcmp(&(fhead->signature), LOONGSON_VBIOS_LINKLIST, 5) == 0) {
			if (parse_vbios(fhead) != 0) {
				pr_warn("parse vbios failed\n");
				return -EPERM;
			}
		} else if (memcmp(&(fhead->signature), LOONGSON_SCREENINFO_LINKLIST, 5) == 0) {
			if (parse_screeninfo(fhead) != 0) {
				pr_warn("parse screeninfo failed\n");
				return -EPERM;
			}
		}
		fhead = fhead->next;
	}

	return 0;
}

static int get_bpi_version(void *signature)
{
	return 1;
}

void __init fw_init_environ(void)
{
	efi_bp = (struct bootparamsinterface *)_fw_envp;
	loongson_sysconf.bpi_ver = get_bpi_version(&efi_bp->signature);

#ifdef CONFIG_SMP
	register_addrs_set(smp_group, TO_UNCAC(0x1fe01000), 16);
#endif
	register_addrs_set(loongson_chipcfg, TO_UNCAC(0x1fe00180), 4);
	register_addrs_set(loongson_chiptemp, TO_UNCAC(0x1fe0019c), 4);
	register_addrs_set(loongson_freqctrl, TO_UNCAC(0x1fe001d0), 4);

	if (list_find(efi_bp->extlist))
		pr_warn("Scan bootparam failed\n");
}

static int __init init_cpu_fullname(void)
{
	int cpu;

	if (loongson_sysconf.cpuname && !strncmp(loongson_sysconf.cpuname, "Loongson", 8)) {
		for (cpu = 0; cpu < NR_CPUS; cpu++)
			__cpu_full_name[cpu] = loongson_sysconf.cpuname;
	}
	return 0;
}
arch_initcall(init_cpu_fullname);
