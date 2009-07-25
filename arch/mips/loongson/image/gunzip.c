/* Misc. bootloader code (almost) all platforms can use
 *
 * Author: Johnnie Peters <jpeters@mvista.com>
 * Editor: Tom Rini <trini@mvista.com>
 *
 * Derived from arch/ppc/boot/prep/misc.c
 *
 * Ported by Pete Popov <ppopov@mvista.com> to
 * support mips board(s).  I also got rid of the vga console
 * code.
 *
 * Copyright 2000-2001 MontaVista Software Inc.
 * Copyright (C) 2009 Lemote, Inc. & Institute of Computing Technology
 * Author: Wu Zhangjin <wuzj@lemote.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include "zlib.h"
#include <stdarg.h>

extern char *avail_ram;
extern char *end_avail;

/* debug interface via searil port */
extern void puts(const char *s);
extern void puthex(unsigned long val);

void pause(void)
{
	puts("pause\n");
}

void __exit(void)
{
	puts("__exit\n");
	while(1); 
}

void error(char *x)
{
	puts("\n\n");
	puts(x);
	puts("\n\n -- System halted");

	while(1);	/* Halt */
}

void *zalloc(void *x, unsigned items, unsigned size)
{
	void *p = avail_ram;
	
	size *= items;
	size = (size + 7) & -8;
	avail_ram += size;
	if (avail_ram > end_avail) {
		puts("oops... out of memory\n");
		pause();
	}
	return p;
}

void zfree(void *x, void *addr, unsigned nb)
{
}

#define HEAD_CRC	2
#define EXTRA_FIELD	4
#define ORIG_NAME	8
#define COMMENT		0x10
#define RESERVED	0xe0

#define DEFLATED	8

void gunzip(void *dst, int dstlen, unsigned char *src, int *lenp)
{
	z_stream s;
	int r, i, flags;
	
	/* skip header */
	puts("skip header\n");
	i = 10;
	flags = src[3];
	if (src[2] != DEFLATED || (flags & RESERVED) != 0) {
		puts("bad gzipped data\n");
		__exit();
	}
	if ((flags & EXTRA_FIELD) != 0)
		i = 12 + src[10] + (src[11] << 8);
	if ((flags & ORIG_NAME) != 0)
		while (src[i++] != 0)
			;
	if ((flags & COMMENT) != 0)
		while (src[i++] != 0)
			;
	if ((flags & HEAD_CRC) != 0)
		i += 2;
	if (i >= *lenp) {
		puts("gunzip: ran out of data in header\n");
		__exit();
	}
	
	s.zalloc = zalloc;
	s.zfree = zfree;
	puts("call inflateInit2\n");
	r = inflateInit2(&s, -MAX_WBITS);
	if (r != Z_OK) {
		puts("inflateInit2 returned %d\n");
		__exit();
	}
	s.next_in = src + i;
	s.avail_in = *lenp - i;
	s.next_out = dst;
	s.avail_out = dstlen;
	puts("call inflate\n");
	r = inflate(&s, Z_FINISH);
	if (r != Z_OK && r != Z_STREAM_END) {
		puts("inflate returned %d\n");
		__exit();
	}
	*lenp = s.next_out - (unsigned char *) dst;
	puts("call inflateEnd\n");
	inflateEnd(&s);
}
