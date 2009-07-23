#
# arch/mips/zboot/common/Makefile
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Tom Rini	January 2001
#

EXTRA_CFLAGS := -fno-builtin -I$(TOPDIR)/arch/mips/zboot/include -I$(TOPDIR)/arch/$(ARCH)/zboot/common/include

.c.s:
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -S -o $*.s $<
.s.o:
	$(AS) -o $*.o $<
.c.o:
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -c -o $*.o $<
.S.s:
	$(CPP) $(AFLAGS) -o $*.o $<
.S.o:
	$(CC) $(AFLAGS) -c -o $*.o $<


OBJCOPY_ARGS = -O elf32-tradlittlemips
