#
# arch/mips/zboot/Makefile
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.

# Adapted for MIPS Pete Popov, Dan Malek
#
# Copyright (C) 1994 by Linus Torvalds
# Adapted for PowerPC by Gary Thomas
# modified by Cort (cort@cs.nmt.edu)
#

targets 	:= bzImage

EXTRA_CFLAGS := -I$(TOPDIR)/arch/mips/zboot/include 

CFLAGS := $(ZBOOT_FLAGS)


.c.s:
	$(CC) $(CFLAGS) -S -o $*.s $<
.s.o:
	$(AS) -o $*.o $<
.c.o:
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -c -o $*.o $<
.S.s:
	$(CPP) $(AFLAGS) -o $*.o $<
.S.o:
	$(CC) $(AFLAGS) -c -o $*.o $<

GZIP_FLAGS = -v9f
EXTRA_CFLAGS := $(CFLAGS) \
		-I$(TOPDIR)/arch/$(ARCH)/zboot/include \
		-I$(TOPDIR)/include/asm -I$(TOPDIR)/arch/mips/include -I$(TOPDIR)/arch/mips/include/asm
AFLAGS	+= -D__BOOTER__

BOOT_TARGETS = zImage zImage.initrd zImage.flash zImage.initrd.flash

images/vmlinux.gz: $(TOPDIR)/vmlinux
	$(MAKE) -C images vmlinux.gz

$(BOOT_TARGETS): lib/zlib.a images/vmlinux.gz

ifdef CONFIG_CPU_LOONGSON2F
SUBARCH := $(obj)/lm2f
KBUILD_IMAGE := arch/mips/zboot/lm2f/bzImage
endif

$(obj)/bzImage:
	$(Q)$(MAKE) $(build)=$(SUBARCH)  $(KBUILD_IMAGE)
	
# Do the dirs
clean:
clean-files += lm2f/*.o common/*.o lib/*.o \
               images/bzImage* \
	       images/vmlinux* \
	       images/*srec
