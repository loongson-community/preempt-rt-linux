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

/* These two variables specify the free ram region
 * that can be used for temporary malloc area
 *
 * Here is toally 15M
 */
#define AVAIL_RAM_START CKSEG0ADDR(0x83000000)
#define AVAIL_RAM_END CKSEG0ADDR(0x83f00000)

char *avail_ram;
char *end_avail;
char *zimage_start;

/* The linker tells us where the image is. */
extern unsigned char __image_begin, __image_end;
extern unsigned char __ramdisk_begin, __ramdisk_end;
unsigned long initrd_size;

/* decompress with gunzip */
extern void gunzip(void *, int, unsigned char *, int *);

/* debug interface via searil port */
extern void puts(const char *s);
extern void puthex(unsigned long val);

/* cache fulshing support */
extern void flush_cache_all(void);

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
	 * relocate outself to that address.  __image_being points to
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
	avail_ram = (char *)AVAIL_RAM_START;
	end_avail = (char *)AVAIL_RAM_END;

	/* Display standard Linux/MIPS boot prompt for kernel args */
	puts("Uncompressing Linux at load address ");
	puthex(LOADADDR);
	puts("\n");
	/* I don't like this hard coded gunzip size (fixme) */
	gunzip((void *)LOADADDR, 0x1000000, zimage_start, &zimage_size);
#if 1
	flush_cache_all();
#endif
	puts("Now booting the kernel...\n");
}
