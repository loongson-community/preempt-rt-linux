# Makefile for RT specific drivers

ifdef CONFIG_FUNCTION_TRACER
CFLAGS_REMOVE_backfire.o = -pg
CFLAGS_REMOVE_sched_clock_latency.o = -pg
CFLAGS_REMOVE_interrupt_latency.o = -pg
CFLAGS_REMOVE_blocksys.o = -pg
endif

obj-$(CONFIG_BACKFIRE) += backfire.o
obj-$(CONFIG_SCHED_CLOCK_LAT) += sched_clock_latency.o
obj-$(CONFIG_INTR_LAT) += interrupt_latency.o
obj-$(CONFIG_BLOCKSYS) += blocksys.o
