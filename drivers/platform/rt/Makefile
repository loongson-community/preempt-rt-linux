# Makefile for RT specific drivers

ifdef CONFIG_FUNCTION_TRACER
ORIG_CFLAGS := $(KBUILD_CFLAGS)
KBUILD_CFLAGS = $(subst -pg,,$(ORIG_CFLAGS))
endif

obj-$(CONFIG_BACKFIRE) += backfire.o
obj-$(CONFIG_SCHED_CLOCK_LAT) += sched_clock_latency.o
obj-$(CONFIG_SCHED_CLOCK_OVERHEAD) += sched_clock_overhead.o
obj-$(CONFIG_INTR_LAT) += interrupt_latency.o
obj-$(CONFIG_BLOCKSYS) += blocksys.o
