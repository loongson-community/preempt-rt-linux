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
#include <asm/cpu.h>

#include <loongson.h>

void __init prom_init(void)
{
	/* init mach type, does we need to init it?? */
	mips_machtype = PRID_IMP_LOONGSON2;

	/* init several base address */
	set_io_port_base((unsigned long)
			 ioremap(LOONGSON_PCIIO_BASE, LOONGSON_PCIIO_SIZE));

	prom_init_cmdline();
	prom_init_memory();
}

void __init prom_free_prom_memory(void)
{
}
