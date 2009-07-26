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

# Assume you have at least a 256M memory, this is used in cache.c
KBUILD_CFLAGS := $(LINUXINCLUDE) $(KBUILD_CFLAGS) -D__KERNEL__ -DMEM_SIZE_M=256 -DSERIAL_PORT_DEBUG
KBUILD_AFLAGS := $(LINUXINCLUDE) $(KBUILD_AFLAGS) -D__ASSEMBLY__ \
	-DKERNEL_ENTRY=0x$(shell $(NM) $(objtree)/$(KBUILD_IMAGE) | grep kernel_entry | cut -f1 -d \ )

OBJECTS := $(obj)/head.o $(obj)/decompress.o $(obj)/mem.o $(obj)/cache.o $(obj)/dbg.o

OBJCOPYFLAGS_vmlinux.bin := $(OBJCOPYFLAGS) -O binary -R .comment -S
$(obj)/vmlinux.bin: $(KBUILD_IMAGE)
	$(call if_changed,objcopy)

suffix_$(CONFIG_KERNEL_GZIP)  = gz
suffix_$(CONFIG_KERNEL_BZIP2) = bz2
suffix_$(CONFIG_KERNEL_LZMA)  = lzma
tool_$(CONFIG_KERNEL_GZIP)    = gzip
tool_$(CONFIG_KERNEL_BZIP2)   = bzip2
tool_$(CONFIG_KERNEL_LZMA)    = lzma
$(obj)/vmlinux.$(suffix_y): $(obj)/vmlinux.bin
	$(call if_changed,$(tool_y))

$(obj)/piggy.o: $(obj)/vmlinux.$(suffix_y) $(obj)/dummy.o
	$(Q)$(OBJCOPY) $(OBJCOPYFLAGS) \
		--add-section=.image=$< \
		--set-section-flags=.image=contents,alloc,load,readonly,data \
		$(obj)/dummy.o $@

# The start address of the compressed kernel, Z_KERNEL_START > KERNEL_START + KERNEL_SIZE
LDFLAGS_vmlinuz := $(LDFLAGS) -Ttext 0x$(if $(CONFIG_64BIT),ffffffff,)81000000 -T
$(obj)/vmlinuz: $(src)/ld.script $(OBJECTS) $(obj)/piggy.o
	$(call if_changed,ld)

OBJCOPYFLAGS_zImage := $(OBJCOPYFLAGS) -R .comment -R .stab -R .stabstr -R .initrd -R .sysmap
$(obj)/zImage: $(obj)/vmlinuz
	$(call if_changed,objcopy)

zImage: $(obj)/zImage
	$(Q)ln -sf $< $(objtree)/vmlinuz
	$(Q)echo "  SYMLINK $(objtree)/"vmlinuz

clean:
clean-files += *.o \
               zImage* \
	       vmlinu* \
	       $(objtree)/vmlinuz
