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
# Copyright (C) 2009 Lemote Inc. & Insititute of Computing Technology
# Author: Wu Zhangjin <wuzj@lemote.com>
#

# please remove the comment to enable serial port debug
DEBUG = -DSERIAL_PORT_DEBUG

# Assume you have at least a 256M memory, this is used in cache.c
MEM_SIZE = -DMEM_SIZE_M=256

KERNEL_IMAGE = vmlinux
KERNEL_ENTRY := -DKERNEL_ENTRY=0x$(shell $(NM) $(objtree)/$(KERNEL_IMAGE) | grep kernel_entry | cut -f1 -d \ )
KERNEL_START := -DLOADADDR=0x$(shell $(NM) $(objtree)/$(KERNEL_IMAGE) | grep " _text" | cut -f1 -d \ )

# The start address of the compressed kernel, Z_KERNEL_START > KERNEL_START + KERNEL_SIZE
ifdef CONFIG_32BIT
Z_KERNEL_START := -Ttext 0x81000000 
endif
ifdef CONFIG_64BIT
Z_KERNEL_START := -Ttext 0xffffffff81000000 
endif

ZBOOT_CFLAGS := $(LINUXINCLUDE) $(KBUILD_AFLAGS) -I$(srctree)/include -I$(srctree)/include/linux
OBJCOPY_ARGS := $(OBJCOPYFLAGS) 
LD_ARGS = -L$(srctree)/$(src) -Tld.script $(Z_KERNEL_START) -Bstatic $(LDFLAGS)

OBJECTS := $(obj)/head.o \
	$(obj)/decompress.o \
	$(obj)/gunzip.o \
	$(obj)/zlib.o \
	$(obj)/memcpy.o \
	$(obj)/cache.o \
	$(obj)/dbg.o

$(obj)/head.o: $(src)/head.S $(KERNEL_IMAGE)
	$(Q)$(CC) $(ZBOOT_CFLAGS) $(KERNEL_ENTRY) -c -o $@ $<

$(obj)/%.o: $(src)/%.c
	$(Q)$(CC) $(ZBOOT_CFLAGS) $(KERNEL_START) $(MEM_SIZE) $(DEBUG) -c -o $@ $<

$(obj)/vmlinux.gz: $(KERNEL_IMAGE)
	$(Q)echo "  GZIP   " vmlinux
	$(Q)$(OBJCOPY) $(OBJCOPY_ARGS) -O binary --strip-all $< $(obj)/vmlinux
	$(Q)gzip -cf -9 $(obj)/vmlinux > $@

$(obj)/vmlinuz: $(OBJECTS) $(src)/ld.script $(obj)/vmlinux.gz $(obj)/dummy.o
	$(Q)$(OBJCOPY) $(OBJCOPY_ARGS) \
		--add-section=.image=$(obj)/vmlinux.gz \
		--set-section-flags=.image=contents,alloc,load,readonly,data \
		$(obj)/dummy.o $(obj)/image.o
	$(Q)$(LD) $(LD_ARGS) -o $@ $(OBJECTS) $(obj)/image.o 
	$(Q)$(OBJCOPY) $(OBJCOPY_ARGS) $@ $@ -R .comment -R .stab -R .stabstr \
		-R .initrd -R .sysmap

bzImage: $(obj)/vmlinuz
	$(Q)mv $(obj)/vmlinuz $(obj)/bzImage
	$(Q)echo "  GEN     $(obj)/"bzImage
	$(Q)ln -sf $(objtree)/$(obj)/bzImage $(objtree)/vmlinuz
	$(Q)echo "  SYMLINK $(objtree)/"vmlinuz

clean:
clean-files += *.o \
               bzImage* \
	       vmlinux* \
	       $(objtree)/vmlinuz
