/*
 * Based on Ocelot Linux port, which is
 * Copyright 2001 MontaVista Software Inc.
 * Author: jsun@mvista.com or jsun@junsun.net
 *
 * Copyright 2003 ICT CAS
 * Author: Michael Guo <guoyi@ict.ac.cn>
 *
 * Copyright (C) 2007 Lemote Inc. & Insititute of Computing Technology
 * Author: Fuxin Zhang, zhangfx@lemote.com
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/bootmem.h>

#include <asm/bootinfo.h>
#include <asm/mipsregs.h>

#include <loongson.h>

#if defined(CONFIG_CPU_LOONGSON2F) && defined(CONFIG_64BIT)
unsigned long _loongson_addrwincfg_base;

/* Loongson CPU address windows config space base address */
static inline void set_loongson_addrwincfg_base(unsigned long base)
{
	*(unsigned long *)&_loongson_addrwincfg_base = base;
	barrier();
}
#endif

/* stop all perf counters by default
 *   $24 is the control register of loongson perf counter
 */
static inline void stop_perf_counters(void)
{
	__write_64bit_c0_register($24, 0, 0);
}

void __init prom_init(void)
{
	/* init several base address */
	set_io_port_base((unsigned long)
			 ioremap(LOONGSON_PCIIO_BASE, LOONGSON_PCIIO_SIZE));

#if defined(CONFIG_CPU_LOONGSON2F) && defined(CONFIG_64BIT)
	set_loongson_addrwincfg_base((unsigned long)
				     ioremap(LOONGSON_ADDRWINCFG_BASE,
					     LOONGSON_ADDRWINCFG_SIZE));
#endif
	/* stop all perf counters */
	stop_perf_counters();

	prom_init_cmdline();
	prom_init_env();
	prom_init_memory();
}

void __init prom_free_prom_memory(void)
{
}
