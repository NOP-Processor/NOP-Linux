// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020-2021 Loongson Technology Corporation Limited
 */

#include <linux/io.h>
#include <linux/module.h>
#include <linux/serial_8250.h>
#include <loongson.h>

#define PORT(int, clk)			\
{								\
	.irq		= int,					\
	.uartclk	= clk,					\
	.iotype		= UPIO_PORT,				\
	.flags		= UPF_BOOT_AUTOCONF | UPF_SKIP_TEST,	\
	.regshift	= 0,					\
}

#define PORT_M(int, clk)				\
{								\
	.irq		= 50 + (int),		\
	.uartclk	= clk,					\
	.iotype		= UPIO_MEM,				\
	.membase	= (void __iomem *)NULL,			\
	.flags		= UPF_BOOT_AUTOCONF | UPF_SKIP_TEST,	\
	.regshift	= 0,					\
}

static struct plat_serial8250_port uart8250_data[][3] = {
	[0]	= {PORT_M(2, 33000000), {} },
	[1] = {PORT_M(2, 33000000), {} },
	[2]	= {},
};

static struct platform_device uart8250_device = {
	.name = "serial8250",
	.id = PLAT8250_DEV_PLATFORM,
};

extern unsigned long _loongson_uart_base[];
extern unsigned long loongson_uart_base[];

static int __init serial_init(void)
{
	unsigned char iotype;

	iotype = uart8250_data[1][0].iotype;

		uart8250_data[1][0].mapbase =
			loongson_uart_base[0];
		uart8250_data[1][0].membase =
			(void __iomem *)_loongson_uart_base[0];
		uart8250_data[1][0].uartclk =0x10000000;

	memset(&uart8250_data[1][0],
			0, sizeof(struct plat_serial8250_port));
	uart8250_device.dev.platform_data = uart8250_data[1];

	return platform_device_register(&uart8250_device);
}
module_init(serial_init);

static void __init serial_exit(void)
{
	platform_device_unregister(&uart8250_device);
}
module_exit(serial_exit);
