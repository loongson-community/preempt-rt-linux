#
# Makefile for loongson based machines.
#

obj-y += setup.o init.o cmdline.o env.o time.o reset.o irq.o \
	pci.o bonito-irq.o mem.o misc.o

#
# Early printk support
#
obj-$(CONFIG_EARLY_PRINTK) += early_printk.o

#
# Enable RTC Class support
#
# please enable CONFIG_RTC_DRV_CMOS
#
obj-$(CONFIG_RTC_DRV_CMOS) += rtc.o

#
# Enable CS5536 Virtual Support Module(VSM) for virtulize the PCI configure
# space
#
obj-$(CONFIG_CS5536) += cs5536/

EXTRA_CFLAGS += -Werror
