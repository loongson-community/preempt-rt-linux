/*
 * Misc. bootloader code for many machines.
 *
 * Copyright 2001 MontaVista Software Inc.
 * Author: Matt Porter <mporter@mvista.com> Derived from
 * arch/ppc/boot/prep/misc.c
 *
 * Copyright (C) 2009 Lemote, Inc. & Institute of Computing Technology
 * Author: Wu Zhangjin <wuzj@lemote.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <asm/addrspace.h>
#include <linux/types.h>
#include <linux/kernel.h>

/* These two variables specify the free mem region
 * that can be used for temporary malloc area
 *
 * Here is toally 15M
 */
#define FREE_MEM_START CKSEG0ADDR(0x83000000)
#define FREE_MEM_END CKSEG0ADDR(0x83f00000)

unsigned long free_mem_ptr;
unsigned long free_mem_end_ptr;
char *zimage_start;

/* The linker tells us where the image is. */
extern unsigned char __image_begin, __image_end;
extern unsigned char __ramdisk_begin, __ramdisk_end;
unsigned long initrd_size;

/* debug interface via searil port */
extern void puts(const char *s);
extern void puthex(unsigned long val);
void error(char *x)
{
	puts("\n\n");
	puts(x);
	puts("\n\n -- System halted");

	while (1)
		;	/* Halt */
}

/* cache fulshing support */
extern void flush_cache_all(void);

/* gunzip declarations */
#define STATIC static

#ifdef CONFIG_KERNEL_GZIP
#include "../../../../lib/decompress_inflate.c"
#elif defined(CONFIG_KERNEL_BZIP2)
#include "../../../../lib/decompress_bunzip2.c"
#elif defined(CONFIG_KERNEL_LZMA)
#include "../../../../lib/decompress_unlzma.c"
#endif

void decompress_kernel(unsigned long load_addr, int num_words,
		       unsigned long cksum, unsigned long *sp)
{
	extern unsigned long start;
	int zimage_size;

	initrd_size = (unsigned long)(&__ramdisk_end) -
	    (unsigned long)(&__ramdisk_begin);

	/*
	 * Reveal where we were loaded at and where we
	 * were relocated to.
	 */
	puts("loaded at:     ");
	puthex(load_addr);
	puts(" ");
	puthex((unsigned long)(load_addr + (4 * num_words)));
	puts("\n");
	if ((unsigned long)load_addr != (unsigned long)&start) {
		puts("relocated to:  ");
		puthex((unsigned long)&start);
		puts(" ");
		puthex((unsigned long)((unsigned long)&start +
				       (4 * num_words)));
		puts("\n");
	}

	/*
	 * We link ourself to an arbitrary low address.  When we run, we
	 * relocate outself to that address.  __image_beign points to
	 * the part of the image where the zImage is. -- Tom
	 */
	zimage_start = (char *)(unsigned long)(&__image_begin);
	zimage_size = (unsigned long)(&__image_end) -
	    (unsigned long)(&__image_begin);

	/*
	 * The zImage and initrd will be between start and _end, so they've
	 * already been moved once.  We're good to go now. -- Tom
	 */
	puts("zimage at:     ");
	puthex((unsigned long)zimage_start);
	puts(" ");
	puthex((unsigned long)(zimage_size + zimage_start));
	puts("\n");

	if (initrd_size) {
		puts("initrd at:     ");
		puthex((unsigned long)(&__ramdisk_begin));
		puts(" ");
		puthex((unsigned long)(&__ramdisk_end));
		puts("\n");
	}

	/* assume the chunk below 8M is free */
	free_mem_ptr = FREE_MEM_START;
	free_mem_end_ptr = FREE_MEM_END;

	/* Display standard Linux/MIPS boot prompt for kernel args */
	puts("Uncompressing Linux at load address ");
	puthex(VMLINUX_LOAD_ADDRESS);
	puts("\n");
	/* I don't like this hard coded gunzip size (fixme) */
	decompress(zimage_start, zimage_size, 0, 0,
		   (void *)VMLINUX_LOAD_ADDRESS, 0, error);
#if 1
	flush_cache_all();
#endif
	puts("Now, booting the kernel...\n");
}
