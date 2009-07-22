/*
 * arch/mips/zboot/common/misc-simple.c
 *
 * Misc. bootloader code for many machines.  This assumes you have are using
 * a 6xx/7xx/74xx CPU in your machine.  This assumes the chunk of memory
 * below 8MB is free.  Finally, it assumes you have a NS16550-style uart for 
 * your serial console.  If a machine meets these requirements, it can quite
 * likely use this code during boot.
 * 
 * Author: Matt Porter <mporter@mvista.com>
 * Derived from arch/ppc/boot/prep/misc.c
 *
 * Copyright 2001 MontaVista Software Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <config.h>
#include <linux/types.h>
#include <asm/page.h>
#include <asm/processor.h>
#include <asm/mmu.h>

#include "zlib.h"

extern struct NS16550 *com_port;

char *avail_ram;
char *end_avail;
extern char _end[];
char *zimage_start;

#ifdef CONFIG_CMDLINE
#define CMDLINE CONFIG_CMDLINE
#else
#define CMDLINE ""
#endif
char cmd_preset[] = CMDLINE;
char cmd_buf[256];
char *cmd_line = cmd_buf;

/* The linker tells us where the image is.
*/
extern unsigned char __image_begin, __image_end;
extern unsigned char __ramdisk_begin, __ramdisk_end;
unsigned long initrd_size;

extern void puts(const char *);
extern void putc(const char c);
extern void puthex(unsigned long val);
extern void *memcpy(void * __dest, __const void * __src,
			    __kernel_size_t __n);
extern void gunzip(void *, int, unsigned char *, int *);
extern void udelay(long delay);
extern int tstc(void);
extern int getc(void);
extern volatile struct NS16550 *serial_init(int chan);

void
decompress_kernel(unsigned long load_addr, int num_words, 
		unsigned long cksum, unsigned long *sp)
{
	extern unsigned long start;
	int	zimage_size;

//	com_port = (struct NS16550 *)serial_init(0);

	initrd_size = (unsigned long)(&__ramdisk_end) -
		(unsigned long)(&__ramdisk_begin);

	/*
	 * Reveal where we were loaded at and where we
	 * were relocated to.
	 */
	puts("loaded at:     "); puthex(load_addr);
	puts(" "); puthex((unsigned long)(load_addr + (4*num_words))); puts("\n");
	if ( (unsigned long)load_addr != (unsigned long)&start )
	{
		puts("relocated to:  "); puthex((unsigned long)&start);
		puts(" ");
		puthex((unsigned long)((unsigned long)&start + (4*num_words)));
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
	puts("zimage at:     "); puthex((unsigned long)zimage_start);
	puts(" "); puthex((unsigned long)(zimage_size+zimage_start));
	puts("\n");

	if ( initrd_size ) {
		puts("initrd at:     ");
		puthex((unsigned long)(&__ramdisk_begin));
		puts(" "); puthex((unsigned long)(&__ramdisk_end));puts("\n");
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
	puts("Now booting the kernel\n");
}
