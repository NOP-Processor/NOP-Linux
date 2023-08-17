/*
 * Copyright (C) 2009 Lemote Inc.
 * Author: Wu Zhangjin, wuzhangjin@gmail.com
 *
 * This program is free software; you can redistribute	it and/or modify it
 * under  the terms of	the GNU General	 Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/export.h>
#include <asm/bootinfo.h>

#include <loongson.h>

/* raw */
unsigned long loongson_uart_base[3] = {};
/* ioremapped */
unsigned long _loongson_uart_base[3] = {};

EXPORT_SYMBOL(loongson_uart_base);
EXPORT_SYMBOL(_loongson_uart_base);

void prom_init_loongson_uart_base(void)
{
	loongson_uart_base[0] =(unsigned long ) 0x9fe001e0;
	_loongson_uart_base[0] =
		(unsigned long)ioremap(loongson_uart_base[0], 8);
}
