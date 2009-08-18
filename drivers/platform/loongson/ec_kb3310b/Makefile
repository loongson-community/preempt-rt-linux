#
# Embedded Controller(kb3310b) Driver for Yeeloong laptop
#

obj-$(CONFIG_EC_KB3310B) += ec_miscd.o ec_batd.o ec_ftd.o ec_scid.o

ec_miscd-objs	:= ec_misc.o
ec_batd-objs	:= ec_bat.o
ec_ftd-objs	:= ec_ft.o
ec_scid-objs	:= ec_sci.o

obj-$(CONFIG_EC_KB3310B_DEBUG) += io_msr_debugd.o

io_msr_debugd-objs	:= io_msr_debug.o
