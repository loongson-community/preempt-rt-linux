#
# Makefile for the Gdium-specific kernel support
#

obj-y += reset.o setup.o platform.o serial.o

obj-$(CONFIG_MFD_SM501) += gclock.o

obj-$(CONFIG_EARLY_PRINTK) += dbg_io.o

obj-$(CONFIG_HIBERNATION) += hibernate.o
obj-$(CONFIG_SUSPEND) += suspend.o
