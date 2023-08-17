/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Author: Hanlu Li <lihanlu@loongson.cn>
 *         Huacai Chen <chenhuacai@loongson.cn>
 *
 * Copyright (C) 2020-2021 Loongson Technology Corporation Limited
 */
#ifdef CONFIG_32BIT
#define __ARCH_WANT_STAT64
#define __ARCH_WANT_TIME32_SYSCALLS
#endif

#include <uapi/asm/unistd.h>

#define TRANS_ARCH_MASK                 0xffff0000
#define SYS_NUM_MASK                    0xffff
#define NR_syscalls (__NR_syscalls)
