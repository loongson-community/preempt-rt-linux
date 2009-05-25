#
# Makefile for Lemote Fulong mini-PC board.
#

obj-y += setup.o init.o cmdline.o time.o reset.o irq.o \
	pci.o bonito-irq.o mem.o misc.o

#
# Early printk support
#
obj-$(CONFIG_EARLY_PRINTK) += early_printk.o

EXTRA_CFLAGS += -Werror
