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
 * Copyright (C) 2009 Lemote Inc. & Insititute of Computing Technology
 * Author: Wu Zhangjin, wuzj@lemote.com
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/bootmem.h>

#include <asm/bootinfo.h>
#include <loongson.h>
#include <cmdline.h>

unsigned long bus_clock, cpu_clock_freq;
unsigned long memsize, highmemsize;

int prom_argc;
/* pmon passes arguments in 32bit pointers */
int *_prom_argv, *_prom_envp;

#define parse_even_earlier(res, option, p)				\
do {									\
	if (strncmp(option, (char *)p, strlen(option)) == 0)		\
			strict_strtol((char *)p + strlen(option"="),	\
					10, &res);			\
} while (0)

void __init prom_init_cmdline(void)
{
	int i;
	long l;
	prom_argc = fw_arg0;
	_prom_argv = (int *)fw_arg1;
	_prom_envp = (int *)fw_arg2;

	/* arg[0] is "g", the rest is boot parameters */
	arcs_cmdline[0] = '\0';
	for (i = 1; i < prom_argc; i++) {
		l = (long)_prom_argv[i];
		if (strlen(arcs_cmdline) + strlen(((char *)l) + 1)
		    >= sizeof(arcs_cmdline))
			break;
		strcat(arcs_cmdline, ((char *)l));
		strcat(arcs_cmdline, " ");
	}

	/* machine specific prom_init_cmdline */
	mach_prom_init_cmdline();

	/* handle console, root, busclock, cpuclock, memsize, highmemsize
	arguments */

	if ((strstr(arcs_cmdline, "console=")) == NULL)
		strcat(arcs_cmdline, " console=ttyS0,115200");
	if ((strstr(arcs_cmdline, "root=")) == NULL)
		strcat(arcs_cmdline, " root=/dev/hda1");

	l = (long)*_prom_envp;
	while (l != 0) {
		parse_even_earlier(bus_clock, "busclock", l);
		parse_even_earlier(cpu_clock_freq, "cpuclock", l);
		parse_even_earlier(memsize, "memsize", l);
		parse_even_earlier(highmemsize, "highmemsize", l);
		_prom_envp++;
		l = (long)*_prom_envp;
	}
	if (memsize == 0)
		memsize = 256;

	pr_info("busclock=%ld, cpuclock=%ld, memsize=%ld, highmemsize=%ld\n",
		bus_clock, cpu_clock_freq, memsize, highmemsize);
}
