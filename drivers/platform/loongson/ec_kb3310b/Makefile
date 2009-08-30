#
# Embedded Controller(kb3310b) Driver for Yeeloong laptop
#

obj-$(CONFIG_EC_KB3310B) += \
	ec_miscd.o \
	ec_batd.o \
	ec_scid.o \
	yeeloong_laptopd.o

ec_miscd-objs	:= ec_misc.o
ec_batd-objs	:= ec_bat.o
ec_scid-objs	:= ec_sci.o
yeeloong_laptopd-objs	:= yeeloong_laptop.o

obj-$(CONFIG_EC_KB3310B_DEBUG) += io_msr_debugd.o

io_msr_debugd-objs	:= io_msr_debug.o
