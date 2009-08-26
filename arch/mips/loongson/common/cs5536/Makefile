#
# Makefile for CS5536 support.
#

#
# core modules
#
obj-$(CONFIG_CS5536) += cs5536_pci.o cs5536_ide.o cs5536_acc.o cs5536_ohci.o \
			cs5536_isa.o cs5536_ehci.o
#
# selective modules
#
# add a CS5536_[MODULE] to your board in arch/mips/loongson/Kconfig
#
obj-$(CONFIG_CS5536_FLASH) += cs5536_flash.o
obj-$(CONFIG_CS5536_NOR_FLASH) += cs5536_flash.o
obj-$(CONFIG_CS5536_OTG) += cs5536_otg.o
obj-$(CONFIG_CS5536_UDC) += cs5536_udc.o

#
# Enable cs5536 mfgpt Timer
#
obj-$(CONFIG_CS5536_MFGPT) += cs5536_mfgpt.o

#
# Enable cs5536 I2C/SMBUS support
#
obj-$(CONFIG_CS5536_I2C) += cs5536_i2c.o


EXTRA_CFLAGS += -Werror
