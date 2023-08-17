/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020-2021 Loongson Technology Corporation Limited
 */
#include <linux/kernel.h>
#include <linux/console.h>
#include <linux/printk.h>
#include <linux/init.h>
#include <asm/setup.h>

extern void prom_putchar(char);
extern struct console *early_console;

static void early_console_write(struct console *con, const char *s, unsigned n)
{
	while (n-- && *s) {
		if (*s == '\n')
			prom_putchar('\r');
		prom_putchar(*s);
		s++;
	}
}

static struct console early_console_prom = {
	.name	= "early",
	.write	= early_console_write,
	.flags	= CON_PRINTBUFFER | CON_BOOT,
	.index	= -1
};

void __init setup_early_printk(void)
{
	if (early_console)
		return;
	early_console = &early_console_prom;
	register_console(&early_console_prom);
}
