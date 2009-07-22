# arch/mips/zboot/lm2f/Makefile
# 
# Makefile for Alchemy Semiconductor Pb1[015]00 boards.
# All of the boot loader code was derived from the ppc
# boot code.
#
# Copyright 2001,2002 MontaVista Software Inc.
#
# Author: Mark A. Greer
#	  mgreer@mvista.com
# Ported and modified for mips support by 
# Pete Popov <ppopov@mvista.com>
#
# This program is free software; you can redistribute  it and/or modify it
# under  the terms of  the GNU General  Public License as published by the
# Free Software Foundation;  either version 2 of the  License, or (at your
# option) any later version.

#########################################################################
# START BOARD SPECIFIC VARIABLES
BNAME=lm2f

# These two variables control where the zImage is stored
# in flash and loaded in memory.  It only controls how the srec
# file is generated, the code is the same.
ZBOOT_CFLAGS := -fno-pic -pipe -msoft-float -ffreestanding  -march=r4600 -Wa,--trap -G 0 -mno-abicalls  
ZBOOT_CFLAGS += -D__KERNEL__
ZBOOT_CFLAGS += -I$(TOPDIR)/include -I$(TOPDIR)/arch/mips/include -I$(TOPDIR)/arch/mips/include/asm/mach-loongson/ -I$(TOPDIR)/arch/mips/include/asm/ -I$(TOPDIR)/arch/mips/include/asm/mach-generic -I$(TOPDIR)/arch/mips/zboot/include
ZBOOT_CFLAGS += -fno-builtin -isystem
ZBOOT_AFLAGS := -fno-pic -pipe -msoft-float -ffreestanding  -march=r4600 -Wa,--trap 

RAM_RUN_ADDR = 0x81000000
FLASH_LOAD_ADDR = 0xBFD00000

# These two variables specify the free ram region
# that can be used for temporary malloc area
AVAIL_RAM_START=0x83000000
AVAIL_RAM_END=0x83f00000

# This one must match the LOADADDR in arch/mips/Makefile!
LOADADDR=0x80200000
# END BOARD SPECIFIC VARIABLES
#########################################################################

targets 	:= bzImage

libs-y  := $(obj)/../lib
obj-y	+= $(obj)/../images
OBJECTS := $(obj)/head.o $(obj)/../common/misc-common.o $(obj)/../common/misc-simple.o \
		$(obj)/ns16550.o $(obj)/../common/ctype.o $(obj)/../lib/zlib.o \
		$(obj)/../common/string.o

ENTRY := $(obj)/../utils/entry
OFFSET := ../utils/offset
SIZE := ../utils/size

LD_ARGS := -T $(obj)/../ld.script -Ttext $(RAM_RUN_ADDR) -Bstatic
OBJCOPY_ARGS = -O elf32-tradlittlemips

all: bzImage


$(obj)/head.o:  $(obj)/head.S $(TOPDIR)/vmlinux
	$(Q)$(CC) $(ZBOOT_CFLAGS) -I$(TOPDIR)/include -D__ASSEMBLY__ \
	  -DKERNEL_ENTRY=$(shell sh $(ENTRY) $(NM) $(TOPDIR)/vmlinux )	-c -o $@ $<

$(obj)/../common/misc-common.o:  $(obj)/../common/misc-common.c
	$(Q)$(CC) $(ZBOOT_CFLAGS) -I$(TOPDIR)/include -c -o $@ $<

$(obj)/../common/ctype.o: $(obj)/../common/ctype.c
	$(Q)$(CC) $(ZBOOT_CFLAGS) -I$(TOPDIR)/include -c -o $@ $< 

$(obj)/../common/string.o: $(obj)/../common/string.c
	$(Q)$(CC) $(ZBOOT_CFLAGS) -I$(TOPDIR)/include -c -o $@ $< 

$(obj)/../common/dummy.o: $(obj)/../common/dummy.c
	$(Q)$(CC) $(ZBOOT_CFLAGS) -I$(TOPDIR)/include -c -o $@ $< 

$(obj)/../lib/zlib.o: $(obj)/../lib/zlib.c
	$(Q)$(CC) $(ZBOOT_CFLAGS) -I$(TOPDIR)/include -c -o $@ $< 

$(obj)/../common/misc-simple.o: $(obj)/../common/misc-simple.c
	$(Q)$(CC)  $(ZBOOT_CFLAGS) \
	   	-I$(TOPDIR)/include/asm/mach-generic -I$(TOPDIR)/include/asm/mach-mips  \
		-DINITRD_OFFSET=0 -DINITRD_SIZE=0 -DZIMAGE_OFFSET=0 \
		-DAVAIL_RAM_START=$(AVAIL_RAM_START) \
		-DAVAIL_RAM_END=$(AVAIL_RAM_END) \
		-DLOADADDR=$(LOADADDR) \
		-DZIMAGE_SIZE=0 -c -o $@ $<
ifeq (1,0)
$(obj)/../common/misc-simple.o: $(obj)/../common/misc-simple.c
	$(CC) $(ZBOOT_CFLAGS)  -I$(TOPDIR)/include/asm/mach-generic -I$(TOPDIR)/include/asm/mach-mips  -DINITRD_OFFSET=0 -DINITRD_SIZE=0 -DZIMAGE_OFFSET=0 \
		-DAVAIL_RAM_START=$(AVAIL_RAM_START) \
		-DAVAIL_RAM_END=$(AVAIL_RAM_END) \
		-DLOADADDR=$(LOADADDR) \
		-DZIMAGE_SIZE=0 -c -o $@ $<
endif
$(obj)/ns16550.o: $(obj)/ns16550.c
	$(Q)$(CC) $(ZBOOT_CFLAGS) -I$(TOPDIR)/include -c -o $@ $<

$(obj)/../lib/lib.a:
	$(MAKE) $(build)=$(dir $@) 

$(obj)/../images/vmlinux.gz: $(TOPDIR)/vmlinux
	$(Q)$(MAKE) $(build)=$(dir $@) vmlinux.gz
zvmlinux: $(OBJECTS) $(LIBS) $(obj)/../ld.script $(obj)/../images/vmlinux.gz $(obj)/../common/dummy.o
	$(Q)$(OBJCOPY) \
		--add-section=.image=$(obj)/../images/vmlinux.gz \
		--set-section-flags=.image=contents,alloc,load,readonly,data \
		$(obj)/../common/dummy.o image.o
	$(Q)$(LD) $(LD_ARGS) -o $@ $(OBJECTS) image.o  
	$(Q)$(OBJCOPY) $(OBJCOPY_ARGS) $@ $@ -R .comment -R .stab -R .stabstr \
		-R .initrd -R .sysmap

# Here we manipulate the image in order to get it the necessary
# srecord file we need.
$(obj)/bzImage: zvmlinux
	$(Q)mv zvmlinux $(obj)/../images/bzImage.$(BNAME)
	$(Q)echo "  GEN    $(obj)/../images/"bzImage.$(BNAME)
	$(Q)$(OBJCOPY) -O srec $(obj)/../images/bzImage.$(BNAME) $(obj)/../images/$(BNAME).srec

zImage.flash: zImage
	$(OBJCOPY) -O srec --adjust-vma 0x3ed00000 \
		../images/zImage.$(BNAME) ../images/$(BNAME).flash.srec
